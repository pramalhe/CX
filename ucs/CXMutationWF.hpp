/*
 * Copyright 2014-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *
 * This work is published under the MIT license. See LICENSE.TXT
 */
#ifndef _CXMUTATIONWF_H_
#define _CXMUTATIONWF_H_

#include <atomic>
#include <stdexcept>
#include <cstdint>
#include <functional>
#include <cassert>
#include <chrono>
#include <thread>

#include "../common/CircularArray.hpp"
#include "../common/HazardPointersCX.hpp"
#include "../common/StrongTryRIRWLock.hpp"

using namespace std;
using namespace chrono;
/**
 * <h1> CXMutation Wait-Free </h1>
 *
 * This is the wait-free implementation of CX, using Turn queue insertion for the mutation queue.
 * We're not using the pure Turn queue algorithm because we don't need the
 * dequeue() and we added a monotonically incrementing ticket in the node.
 *
 * Consistency: Linearizable
 * applyUpdate() progress: wait-free bounded O(N_threads)
 * applyRead() progress: wait-free bounded
 * Memory Reclamation: Hazard Pointers + ORCs
 *
 * Things to improve:
 * - Get rid of CircularArray or make it more flexible;
 * - Activate HPGuard to clear the hazard pointers when leaving;
 * - Use maxThreads in the rwlock of Combined;
 *
 * <h2> Papers </h2>
 * CX paper:
 * github...
 * Turn queue paper:
 * https://dl.acm.org/citation.cfm?id=3019022
 * https://github.com/pramalhe/ConcurrencyFreaks/blob/master/papers/crturnqueue-2016.pdf
 * Hazard Pointers paper:
 * http://web.cecs.pdx.edu/~walpole/class/cs510/papers/11.pdf
 * Strong TryRWLocks paper:
 * https://dl.acm.org/citation.cfm?id=3178519
 */
template<typename C, typename R = bool>  // R must fit in an a std::atomic<R>
class CXMutationWF {

private:
    static const int MAX_READ_TRIES = 10; // Maximum number of times a reader will fail to acquire the shared lock before adding its operation as a mutation
    static const int MAX_THREADS = 128;
    const int maxThreads;

    struct Node {
        std::function<R(C*)>       mutation;
        std::atomic<R>             result;   // This needs to be (relaxed) atomic because there are write-races on it. TODO: change to void*
        std::atomic<Node*>         next {nullptr};
        std::atomic<uint64_t>      ticket {0};
        std::atomic<int>           refcnt {0};
        const int                  enqTid;

        template<typename F> Node(F&& mut, int tid) : mutation{mut}, enqTid{tid} { }
    };

    // Class to combine head and the instance
    struct Combined {
        Node*                      head {nullptr};
        C*                         obj {nullptr};
        StrongTryRIRWLock          rwLock {MAX_THREADS};

        // Helper function to update newComb->head while keeping track of ORCs.
        void updateHead(Node* mn) {
            mn->refcnt.fetch_add(1); // mn is assumed to be protected by an HP
            if (head != nullptr) head->refcnt.fetch_add(-1);
            head = mn;
        }
    };

    alignas(128) std::atomic<Combined*> curComb { nullptr };

    std::function<R(C*)> sentinelMutation = [](C* c){ return R{}; };
    Node* sentinel = new Node(sentinelMutation, 0);
    // The tail of the queue/list of mutations.
    // Starts by pointing to a sentinel/dummy node
    std::atomic<Node*> tail {sentinel};

    alignas(128) Combined* combs;

    // Enqueue requests
    alignas(128) std::atomic<Node*> enqueuers[MAX_THREADS];

    // We need two hazard pointers for the enqueue() (ltail and lnext), one for myNode, and two to traverse the list/queue
    HazardPointersCX<Node> hp {5, maxThreads};
    const int kHpTail     = 0;
    const int kHpTailNext = 1;
    const int kHpHead     = 2;
    const int kHpNext     = 3;
    const int kHpMyNode   = 4;

    CircularArray<Node>* preRetired[MAX_THREADS];

    Combined* getCombined(uint64_t myTicket, const int tid) {
        for (int i = 0; i < maxThreads; i++) {
            Combined* lcomb = curComb.load();
            if (!lcomb->rwLock.sharedTryLock(tid)) continue;
            Node* lhead = lcomb->head;
            uint64_t lticket = lhead->ticket.load();
            if (lticket < myTicket && lhead != lhead->next.load()) return lcomb;
            lcomb->rwLock.sharedUnlock(tid);
            // in case lhead->ticket.load() has been made visible
            if (lticket >= myTicket && lcomb == curComb.load()) return nullptr;
        }
        return nullptr;
    }

    /**
     * Enqueue algorithm from the Turn queue, adding a monotonically incrementing ticket
     * Steps when uncontended:
     * 1. Add node to enqueuers[]
     * 2. Insert node in tail.next using a CAS
     * 3. Advance tail to tail.next
     * 4. Remove node from enqueuers[]
     */
    void enqueue(Node* myNode, const int tid) {
        enqueuers[tid].store(myNode);
        for (int i = 0; i < maxThreads; i++) {
            if (enqueuers[tid].load() == nullptr) {
                //hp.clear(tid);
                return; // Some thread did all the steps
            }
            Node* ltail = hp.protectPtr(kHpTail, tail.load(), tid);
            if (ltail != tail.load()) continue; // If the tail advanced maxThreads times, then my node has been enqueued
            if (enqueuers[ltail->enqTid].load() == ltail) {  // Help a thread do step 4
                Node* tmp = ltail;
                enqueuers[ltail->enqTid].compare_exchange_strong(tmp, nullptr);
            }
            for (int j = 1; j < maxThreads+1; j++) {         // Help a thread do step 2
                Node* nodeToHelp = enqueuers[(j + ltail->enqTid) % maxThreads].load();
                if (nodeToHelp == nullptr) continue;
                Node* nodenull = nullptr;
                ltail->next.compare_exchange_strong(nodenull, nodeToHelp);
                break;
            }
            Node* lnext = ltail->next.load();
            if (lnext != nullptr) {
                hp.protectPtr(kHpTailNext, lnext, tid);
                if (ltail != tail.load()) continue;
                lnext->ticket.store(ltail->ticket.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
                tail.compare_exchange_strong(ltail, lnext); // Help a thread do step 3:
            }
        }
        enqueuers[tid].store(nullptr, std::memory_order_release); // Do step 4, just in case it's not done
    }

public:
    CXMutationWF(C* inst, const int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
        combs = new Combined[2*maxThreads];
        for (int i = 0; i < maxThreads; i++) enqueuers[i].store(nullptr, std::memory_order_relaxed);
        for (int i = 0; i < maxThreads; i++) preRetired[i] = new CircularArray<Node>(hp,i);
        // Start with two valid combined instances (0 and 1), [0] is passed from the constructor and [1] is a copy of it
        combs[0].head = sentinel;
        combs[0].obj = inst;
        combs[1].head = sentinel;
        combs[1].obj = new C(*inst);
        if (maxThreads >= 2) {
            for (int i = 2; i < 4; i++) {
                combs[i].head = sentinel;
                combs[i].obj = new C(*inst);
            }
            sentinel->refcnt.store(4, std::memory_order_relaxed);
        } else {
            sentinel->refcnt.store(2, std::memory_order_relaxed);
        }
        combs[0].rwLock.setReadLock();
        curComb.store(&combs[0]);
    }

    ~CXMutationWF() {
        for (int i = 0; i < 2*maxThreads; i++) {
            if (combs[i].obj == nullptr || combs[i].head == nullptr) continue;
            delete combs[i].obj;
        }
        for (int i = 0; i < maxThreads; i++) delete preRetired[i];
        delete[] combs;
        delete sentinel;
    }

    static std::string className() { return "CXWF-"; }

    /*
     * Adds the mutativeFunc to the queue and applies all mutations up to it, returning the result.
     *
     * Progress Condition: wait-free (bounded by the number of threads)
     *
     * There are several RW-Locks being held throughout the code. For easier
     * reading, we indicate the following logical states:
     * - S: rwlock is being held in Shared mode
     * - X: rwlock is being held in Exclusive mode
     * - H: rwlock is being held in Shared mode, and ready for handover
     * - U: rwlock is not held and Combined instance may now be taken to W
     */
    template<typename F> R applyUpdate(F&& mutativeFunc, const int tid) {
        // Insert our node in the queue
        Node* myNode = new Node(mutativeFunc, tid);
        hp.protectPtrRelease(kHpMyNode, myNode, tid);
        enqueue(myNode, tid);
        const uint64_t myTicket = myNode->ticket.load();
        // Get one of the Combined instances on which to apply mutation(s)
        Combined* newComb = nullptr;
        for (int i = 0; i < 2*maxThreads; i++) {
            if (combs[i].rwLock.exclusiveTryLock(tid)) {
                newComb = &combs[i];
                break;
            }
        } // RWLocks: newComb=X
        if (newComb == nullptr) {
            std::cout << "ERROR: not enough Combined instances\n";
            assert(false);
        }
        Node* mn = newComb->head;
        if (mn != nullptr && mn->ticket.load() >= myTicket) {
            newComb->rwLock.exclusiveUnlock();
            return myNode->result.load();
        }
        Combined* lcomb = nullptr;
        // Apply all mutations starting from 'head' up to our node or the end of the list
        while (mn != myNode) {
            if (mn == nullptr || mn == mn->next.load()) {
                if (lcomb != nullptr || (lcomb = getCombined(myTicket,tid)) == nullptr) {
                    if (mn != nullptr) newComb->updateHead(mn);
                    newComb->rwLock.exclusiveUnlock();
                    return myNode->result.load();
                }
                mn = lcomb->head;
                // Neither the 'instance' nor the 'head' will change now that we hold the shared lock
                newComb->updateHead(mn);
                delete newComb->obj;
                newComb->obj = new C(*lcomb->obj);
                lcomb->rwLock.sharedUnlock(tid);
                continue;
            }
            Node* lnext = hp.protectPtr(kHpHead, mn->next.load(), tid);
            if (mn == mn->next.load()) continue;
            lnext->result.store(lnext->mutation(newComb->obj), std::memory_order_relaxed);
            hp.protectPtrRelease(kHpNext, lnext, tid);
            mn = lnext;
        }
        newComb->updateHead(mn);
        newComb->rwLock.downgrade();  // RWLocks: newComb=H
        // Make the mutation visible to other threads by advancing curComb
        for (int i = 0; i < maxThreads; i++) {
            lcomb = curComb.load();
            if (!lcomb->rwLock.sharedTryLock(tid)) continue;
            if (lcomb->head->ticket.load() >= myTicket) {
                lcomb->rwLock.sharedUnlock(tid);
                if (lcomb != curComb.load()) continue;
                break;
            }
            Combined* tmp = lcomb;
            if (curComb.compare_exchange_strong(tmp, newComb)){
                lcomb->rwLock.setReadUnlock(); // RWLocks: lcomb=U
                // Retire nodes from oldComb->head to newComb->head
                Node* node = lcomb->head;
                lcomb->rwLock.sharedUnlock(tid); // RWLocks: lcomb=H
                while (node != mn) {
                    Node* lnext = node->next.load();
                    preRetired[tid]->add(node);
                    node = lnext;
                }
                return myNode->result.load();
            }
            lcomb->rwLock.sharedUnlock(tid);
        } // RWLocks: newComb=H, lComb=S
        newComb->rwLock.setReadUnlock();
        return myNode->result.load();
    }

    /*
     * Progress Condition: wait-free (bounded by the number of threads)
     */
    template<typename F> R applyRead(F&& readFunc, const int tid) {
        Node* myNode = nullptr;
        for (int i=0; i < MAX_READ_TRIES + maxThreads; i++) {
            Combined* lcomb = curComb.load();
            if (i == MAX_READ_TRIES) { // enqueue read-only operation as if it was a mutation
                myNode = new Node(readFunc, tid);
                hp.protectPtr(kHpMyNode, myNode, tid);
                enqueue(myNode, tid);
            }
            if (lcomb->rwLock.sharedTryLock(tid)) {
                if (lcomb == curComb.load()) {
                    auto ret = readFunc(lcomb->obj);
                    lcomb->rwLock.sharedUnlock(tid);
                    return ret;
                }
                lcomb->rwLock.sharedUnlock(tid);
            }
        }
        return myNode->result.load();
    }
};

#endif /* _CXMUTATION_WF_H_ */

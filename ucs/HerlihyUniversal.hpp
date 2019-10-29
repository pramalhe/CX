/*
 * Copyright 2014-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *
 * This work is published under the MIT license. See LICENSE.TXT
 */
#ifndef _HERLIHY_UNIVERSAL_WF_H_
#define _HERLIHY_UNIVERSAL_WF_H_

#include <atomic>
#include <stdexcept>
#include <cstdint>
#include <functional>
#include <cassert>


/**
 * <h1> Herlihy's Universal wait-free construct </h1>
 *
 * Ported from The Art of Multiprocessor Programming
 *
 * Consistency: Linearizable
 * applyUpdate() progress: wait-free
 * applyRead() progress: wait-free (not specific to reads)
 * Memory Reclamation: none, it leaks memory like crazy
 *
 */
template<typename C>
class HerlihyUniversal {

private:
    static const int MAX_THREADS = 128; // Increase this for the stress tests

    template<typename T>
    class Consensus {
    private:
        static const uint64_t FIRST = -1;
        const int maxThreads;

        alignas(128) std::atomic<uint64_t> r {FIRST};
        alignas(128) std::atomic<T*>* proposed;

        public:
        Consensus(int maxThreads) : maxThreads{maxThreads} {
            proposed = new std::atomic<T*>[maxThreads];
            for (int j=0; j < maxThreads; j++){
                proposed[j].store(nullptr, std::memory_order_relaxed);
            }
        }

        ~Consensus() {
            delete[] proposed;
        }

        // announce my input value to the other threads
        void propose(T* value, int tid) {
            proposed[tid].store(value);
        }

        T* decide(T* value, int tid) {
            propose(value, tid);
            auto tmp = FIRST;
            if (r.compare_exchange_strong(tmp, tid)) // I won
                return proposed[tid].load();
            else // I lost
                return proposed[r.load()].load();
        }
    };


    struct Node {
        std::function<bool(C*)>    mutation; // TODO: change bool to void*
        Consensus<Node>            decideNext{MAX_THREADS}; // decide next Node in list
        std::atomic<bool>          result {false};   // This needs to be (relaxed) atomic because there are write-races on it. TODO: change to void*
        std::atomic<Node*>         next {nullptr};
        std::atomic<uint64_t>      seq {0}; // sequence number

        // TODO: change bool to void*
        Node(std::function<bool(C*)>& mutFunc, int tid) : mutation{mutFunc} { }

        bool casNext(Node* cmp, Node* val) {
            return next.compare_exchange_strong(cmp, val);
        }

        Node* max(std::atomic<Node*>* array) {
            Node* lmax = array[0].load();
            for (int i = 1; i < MAX_THREADS; i++) {
                Node* node = array[i].load();
                if (node == nullptr) continue;
                if (lmax->seq < node->seq) lmax = node;
            }
            return lmax;
        }
    };


    alignas(128) std::atomic<Node*>* announce;
    alignas(128) std::atomic<Node*>* heads;
    alignas(128) C* initialInst;

    std::function<bool(C*)> sentinelMutation = [](C* c){ return false; };
    Node* sentinel = new Node(sentinelMutation, 0);
    // The tail of the queue/list of mutations.
    // Starts by pointing to a sentinel/dummy node
    Node* tail {sentinel};


public:

    HerlihyUniversal(C* inst) {
        initialInst = inst;
        sentinel->seq = 1;
        announce = new std::atomic<Node*>[MAX_THREADS];
        heads = new std::atomic<Node*>[MAX_THREADS];
        for (int i = 0; i < MAX_THREADS; i++) {
            announce[i].store(sentinel, std::memory_order_relaxed);
            heads[i].store(sentinel, std::memory_order_relaxed);
        }
    }


    ~HerlihyUniversal() {
        delete[] announce;
        delete[] heads;
        Node* node = tail;
        while (node != nullptr) {
            Node* prev = node;
            node = node->next.load();
            delete prev;
        }
        delete initialInst;
    }


    static std::string className() { return "HerlihyUniversal-"; }


    //template<typename R>
    bool apply(std::function<bool(C*)>& mutativeFunc, const int tid) { // TODO: change bool to void*/R
        Node* myNode = new Node(mutativeFunc, tid);
        announce[tid].store(myNode);
        heads[tid].store(myNode->max(heads));

        while (announce[tid].load()->seq == 0) {
            Node* before = heads[tid].load();
            Node* help = announce[(int)((before->seq+1) % MAX_THREADS)].load();
            Node* prefer = nullptr;
            if (help->seq == 0) prefer = help;
            else prefer = announce[tid].load();
            Node* after = before->decideNext.decide(prefer, tid);
            before->next.store(after);
            after->seq = before->seq + 1;
            heads[tid].store(after);
        }
        C* myObject = new C(*initialInst);
        Node* current = tail->next.load();
        while (current != announce[tid].load()){
            current->mutation(myObject);
            current = current->next.load();
        }
        heads[tid].store(announce[tid].load());
        auto retval = mutativeFunc(myObject);
        delete myObject;
        return retval;
    }
};



// This class can be used to simplify the usage of sets/multisets with CXMutation.
// For generic code you don't need it (and can't use it), but it can serve as an example of how to use lambdas.
// C must be a set/multiset where the keys are of type CKey
template<typename C, typename CKey>
class HerlihyUniversalSetWF {
private:
    HerlihyUniversal<C> hu{new C()};

public:
    static std::string className() { return "HerlihyUniversal-" + C::className(); }

    // Progress-condition: wait-free
    bool add(CKey* key, const int tid) {
        std::function<bool(C*)> addFunc = [key] (C* set) { return set->add(key); };
        return hu.apply(addFunc, tid);
    }

    // Progress-condition: wait-free
    bool remove(CKey* key, const int tid) {
        std::function<bool(C*)> removeFunc = [key] (C* set) { return set->remove(key); };
        return hu.apply(removeFunc, tid);
    }

    // Progress-condition: wait-free
    bool contains(CKey* key, const int tid) {
        std::function<bool(C*)> containsFunc = [key] (C* set) { return set->contains(key); };
        return hu.apply(containsFunc, tid);
    }

    // Progress-condition: wait-free
    bool iterateAll(std::function<bool(CKey*)> itfun, const int tid) {
        std::function<bool(C*)> func = [&itfun] (C* set) {
        	return set->iterateAll(itfun);
        };
        return hu.applyRead(func, tid);
    }

    // Progress-condition: wait-free
    void addAll(CKey** keys, const int size, const int tid) {
        std::function<bool(C*)> addFunc = [keys,size] (C* set) {
            for (int i = 0; i < size; i++) set->add(keys[i]);
            return true;
        };
        hu.apply(addFunc, tid);
    }
};

#endif /* _HERLIHY_UNIVERSAL_WF_H_ */

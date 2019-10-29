/******************************************************************************
 * Copyright (c) 2014-2016, Pedro Ramalhete, Andreia Correia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Concurrency Freaks nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************
 */

#ifndef _MAGED_MICHAEL_TIM_HARRIS_HASH_SET_HP_H_
#define _MAGED_MICHAEL_TIM_HARRIS_HASH_SET_HP_H_

#include <atomic>
#include <thread>
#include <forward_list>
#include <set>
#include <iostream>
#include <string>
#include <functional>   // For std::hash
#include "common/HazardPointers.hpp"



/**
 * This is a fixed-size hash set where each bucket contains a Maged-Harris linked list set.
 *
 * This is the linked list by Maged M. Michael that uses Hazard Pointers in
 * a correct way because Harris original algorithm with HPs doesn't.
 * Lock-Free Linked List as described in Maged M. Michael paper (Figure 4):
 * http://www.cs.tau.ac.il/~afek/p73-Lock-Free-HashTbls-michael.pdf
 *
 * 
 * <p>
 * This set has three operations:
 * <ul>
 * <li>add(x)      - Lock-Free
 * <li>remove(x)   - Lock-Free
 * <li>contains(x) - Lock-Free
 * </ul><p>
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T> 
class MagedHarrisHashSetHP {

private:
    struct Node {
        T key;
        std::atomic<Node*> next;

        Node(T key) : key{key}, next{nullptr} { }

        bool casNext(Node *cmp, Node *val) {
            return next.compare_exchange_strong(cmp, val);
        }
    };

    // The hash table has a fixed size of 1024 buckets
    static const uint64_t NUM_BUCKETS = 1024;

    // Pointers to head and tail sentinel nodes of the lists
    std::atomic<Node*> head[NUM_BUCKETS];
    std::atomic<Node*> tail[NUM_BUCKETS];

    const int maxThreads;

    // We need 3 hazard pointers
    HazardPointers<Node> hp {3, maxThreads};
    const int kHp0 = 0; // Protects next
    const int kHp1 = 1; // Protects curr
    const int kHp2 = 2; // Protects prev

public:

    MagedHarrisHashSetHP(const int maxThreads) : maxThreads{maxThreads} {
        for (uint64_t ib = 0; ib < NUM_BUCKETS; ib++) {
            head[ib].store(new Node({}));
            tail[ib].store(new Node({}));
            head[ib].load()->next.store(tail[ib].load());
        }
    }

    // We don't expect the destructor to be called if this instance can still be in use
    ~MagedHarrisHashSetHP() {
        for (uint64_t ib = 0; ib < NUM_BUCKETS; ib++) {
            Node *prev = head[ib].load();
            Node *node = prev->next.load();
            while (node != nullptr) {
                delete prev;
                prev = node;
                node = prev->next.load();
            }
            delete prev;
        }
    }

    static std::string className() { return "MagedHarris-HashSetHP"; }

    /*
     * This function is single threaded to be called at the start of the test.
     */
    void addAll(T** keys, const int size, const int tid) {
        for(int i=0;i<size;i++) add(*keys[i], tid);
    }

    /**
     * This method is named 'Insert()' in the original paper.
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     * <p>
     * Progress Condition: Lock-Free
     *
     */
    bool add(T key, const int tid)
    {
        Node *curr, *next;
        std::atomic<Node*> *prev;
        Node* newNode = new Node(key);
        while (true) {
            if (find(&key, &prev, &curr, &next, tid)) {
                delete newNode;              // There is already a matching key
                hp.clear(tid);
                return false;
            }
            newNode->next.store(curr, std::memory_order_relaxed);
            Node *tmp = getUnmarked(curr);
            if (prev->compare_exchange_strong(tmp, newNode)) { // seq-cst
                hp.clear(tid);
                return true;
            }
        }
    }


    /**
     * This method is named 'Delete()' in the original paper.
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     */
    bool remove(T key, const int tid)
    {
        Node *curr, *next;
        std::atomic<Node*> *prev;
        while (true) {
            /* Try to find the key in the list. */
            if (!find(&key, &prev, &curr, &next, tid)) {
                hp.clear(tid);
                return false;
            }
            /* Mark if needed. */
            Node *tmp = getUnmarked(next);
            if (!curr->next.compare_exchange_strong(tmp, getMarked(next))) {
                continue; /* Another thread interfered. */
            }

            tmp = getUnmarked(curr);
            if (prev->compare_exchange_strong(tmp, getUnmarked(next))) { /* Unlink */
                hp.clear(tid);
                hp.retire(getUnmarked(curr), tid); /* Reclaim */
            } else {
                hp.clear(tid);
            }
            /*
             * If we want to prevent the possibility of there being an
             * unbounded number of unmarked nodes, add "else _find(head,key)."
             * This is not necessary for correctness.
             */
            return true;
        }
    }


    /**
     * This is named 'Search()' on the original paper
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     * <p>
     * Progress Condition: Lock-Free
     */
    bool contains(T key, const int tid)
    {
        Node *curr, *next;
        std::atomic<Node*> *prev;
        bool isContains = find(&key, &prev, &curr, &next, tid);
        hp.clear(tid);
        return isContains;
    }


private:

    /**
     * TODO: This needs to be code reviewed... it's not production-ready
     * <p>
     * Progress Condition: Lock-Free
     */
    bool find (T* key, std::atomic<Node*> **par_prev, Node **par_curr, Node **par_next, const int tid)
    {
        std::atomic<Node*> *prev;
        Node *curr, *next;
        auto h = std::hash<T>{}(*key) % NUM_BUCKETS;

     try_again:
        prev = &head[h];
        curr = prev->load();
        // Protect curr with a hazard pointer.
        hp.protectPtr(kHp1, curr, tid);
        if (prev->load() != getUnmarked(curr)) goto try_again;
        while (true) {
            if (getUnmarked(curr) == nullptr) break;
            // Protect next with a hazard pointer.
            next = curr->next.load();
            hp.protectPtr(kHp0, getUnmarked(next), tid);
            if (getUnmarked(curr)->next.load() != next) goto try_again;
            if (getUnmarked(next) == tail[h].load()) break;
            if (prev->load() != getUnmarked(curr)) goto try_again;
            if (getUnmarked(next) == next) { // !cmark in the paper
                if (!(getUnmarked(curr)->key < *key)) { // Check for null to handle head and tail
                    *par_curr = curr;
                    *par_prev = prev;
                    *par_next = next;
                    return (getUnmarked(curr)->key == *key);
                }
                prev = &getUnmarked(curr)->next;
                hp.protectRelease(kHp2, getUnmarked(curr), tid);
            } else {
                // Update the link and retire the node.
                Node *tmp = getUnmarked(curr);
                if (!prev->compare_exchange_strong(tmp, getUnmarked(next))) {
                    goto try_again;
                }
                hp.retire(getUnmarked(curr), tid);
            }
            curr = next;
            hp.protectRelease(kHp1, getUnmarked(next), tid);
        }
        *par_curr = curr;
        *par_prev = prev;
        *par_next = next;
        return false;
    }

    bool isMarked(Node * node) {
    	return ((size_t) node & 0x1);
    }

    Node * getMarked(Node * node) {
    	return (Node*)((size_t) node | 0x1);
    }

    Node * getUnmarked(Node * node) {
    	return (Node*)((size_t) node & (~0x1));
    }
};

#endif /* _MAGED_MICHAEL_TIM_HARRIS_HASH_SET_HP_H_ */

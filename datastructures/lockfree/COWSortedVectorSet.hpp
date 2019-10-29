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

#ifndef _COW_SORTED_VECTOR_SET_H_
#define _COW_SORTED_VECTOR_SET_H_

#include <vector>
#include <iostream>
#include <functional>

#include "../../common/URCUReadersVersion.hpp"
#include "../sequential/SortedVectorSet.hpp"

// TODO: change T* to T&

template<typename T>
class COWSortedVectorSet {

private:
    URCUGraceVersion urcu{128};
    alignas(128) std::atomic<SortedVectorSet<T>*> ptr;

public:
    // Is there a better way of doing this without using constructor/destructor?
    COWSortedVectorSet(const int maxThreads=0) {
        ptr.store(new SortedVectorSet<T>());
    }

    ~COWSortedVectorSet() {
        delete ptr.load();
    }

    std::string className() { return "COW-SortedVectorSet"; }

    // Progress-condition: blocking
    bool add(T* key, const int tid) {
        while(true) {
            urcu.read_lock(tid);
            auto oldptr = ptr.load();
            auto newptr = new SortedVectorSet<T>(*oldptr);
            auto ret = newptr->add(key);
            if (ptr.compare_exchange_weak(oldptr, newptr)) {
                urcu.read_unlock(tid);
                urcu.synchronize();
                delete oldptr;
                return ret;
            }
            delete newptr;
        }
    }

    // Progress-condition: blocking
    bool remove(T* key, const int tid) {
        while(true) {
            urcu.read_lock(tid);
            auto oldptr = ptr.load();
            auto newptr = new SortedVectorSet<T>(*oldptr);
            auto ret = newptr->remove(key);
            if (ptr.compare_exchange_weak(oldptr, newptr)) {
                urcu.read_unlock(tid);
                urcu.synchronize();
                delete oldptr;
                return ret;
            }
            delete newptr;
        }
    }

    // Progress-condition: wait-free
    bool contains(T* key, const int tid) {
        urcu.read_lock(tid);
        auto ret = ptr.load()->contains(key);
        urcu.read_unlock(tid);
        return ret;
    }

    // Progress-condition: blocking
    void addAll(T** keys, const int size, const int tid) {
        while(true) {
            urcu.read_lock(tid);
            auto oldptr = ptr.load();
            auto newptr = new SortedVectorSet<T>(*oldptr);
            for (int i = 0; i < size; i++) newptr->add(keys[i]);
            if (ptr.compare_exchange_weak(oldptr, newptr)) {
                urcu.read_unlock(tid);
                urcu.synchronize();
                delete oldptr;
                return;
            }
            delete newptr;
        }
    }
};

#endif /* _COW_SORTED_VECTOR_SET_H_ */

/******************************************************************************
 * Copyright (c) 2017, Pedro Ramalhete, Andreia Correia
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

#ifndef _WAIT_FREE_RED_BLACK_TREE_H_
#define _WAIT_FREE_RED_BLACK_TREE_H_

#include <vector>
#include <iostream>

#ifdef BENCHRBT
extern void* createData(int nb_threads);
extern bool ins(void *data, uintptr_t key, const int tid);
extern bool del(void *data, uintptr_t key, const int tid);
extern bool trav(void *data, uintptr_t key, const int tid);
#else
void* createData(int nb_threads) { return nullptr; }
bool ins(void *data, uintptr_t key, const int tid) { return false; }
bool del(void *data, uintptr_t key, const int tid) { return false; }
bool trav(void *data, uintptr_t key, const int tid) { std::cout << "Don't call this directly"; return false; }
#endif


template<typename CKey>
class WFRBT {

private:
    void* data;
    // TODO: memory reclamation


public:
    // Is there a better way of doing this without using constructor/destructor?
    WFRBT(const int maxThreads=128) {
        data = ::createData(maxThreads);
        // TODO: add static_assert() that T is of type uintptr_t or equivalent
    }

    std::string className() { return "WFRBT"; }


    bool add(CKey* key, const int tid) {
        return ins(data, (*key).seq, tid); // use *key if CKey is int
    }

    bool remove(CKey* key, const int tid) {
        return !del(data, (*key).seq, tid); // use *key if CKey is int
    }

    bool contains(CKey* key, const int tid) {
        return trav(data, (*key).seq, tid); // use *key if CKey is int
    }

    void addAll(CKey** keys, const int size, const int tid) {
        for (int i = 0; i < size; i++) add(keys[i], tid);
    }

    bool iterateAll(std::function<bool(CKey*)> itfun, const int tid) {
        return false;
    }
};

#endif /* _WFRBT_H_ */

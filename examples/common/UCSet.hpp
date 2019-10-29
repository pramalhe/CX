/******************************************************************************
 * Copyright (c) 2014-2017, Pedro Ramalhete, Andreia Correia
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

#ifndef _UNIVERSAL_CONSTRUCT_SET_H_
#define _UNIVERSAL_CONSTRUCT_SET_H_

#include <functional>

/**
 * <h1> Interface for Universal Constructs (Sets) </h1>
 *
 * UC is the Universal Construct
 * SET is the set class
 * K is the type of the key of the set
 *
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */

// This class can be used to simplify the usage of sets/multisets with CXMutation.
// For generic code you don't need it (and can't use it), but it can serve as an example of how to use lambdas.
// SET must be a set/multiset where the keys are of type K
template<typename UC, typename SET, typename K>
class UCSet {
private:
    static const int MAX_THREADS = 128;
    const int maxThreads;
    UC uc {new SET(), maxThreads};

public:
    UCSet(const int maxThreads=MAX_THREADS) : maxThreads{maxThreads} { }

    static std::string className() { return UC::className() + SET::className(); }

    bool add(K key, const int tid) {
        std::function<bool(SET*)> addFunc = [key] (SET* set) { return set->add(key); };
        return uc.applyUpdate(addFunc, tid);
    }

    bool remove(K key, const int tid) {
        std::function<bool(SET*)> removeFunc = [key] (SET* set) { return set->remove(key); };
        return uc.applyUpdate(removeFunc, tid);
    }

    bool contains(K key, const int tid) {
        std::function<bool(SET*)> containsFunc = [key] (SET* set) { return set->contains(key); };
        return uc.applyRead(containsFunc, tid);
    }

    bool iterateAll(std::function<bool(K*)> itfun, const int tid) {
        std::function<bool(SET*)> func = [&itfun] (SET* set) {
        	return set->iterateAll(itfun);
        };
        return uc.applyRead(func, tid);
    }

    bool iterate(std::function<bool(K*)> itfun, const int tid, uint64_t itersize, K beginkey) {
        std::function<bool(SET*)> func = [&itfun,&itersize,&beginkey] (SET* set) {
        	return set->iterate(itfun, itersize, beginkey);
        };
        return uc.applyRead(func, tid);
    }

    void addAll(K** keys, const int size, const int tid) {
        std::function<bool(SET*)> addFunc = [keys,size] (SET* set) {
            for (int i = 0; i < size; i++) set->add(*keys[i]);
            return true;
        };
        uc.applyUpdate(addFunc, tid);
    }
};

#endif /* _UNIVERSAL_CONSTRUCT_SET_H_ */

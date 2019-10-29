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

#ifndef _CRWWP_UNIVERSAL_H_
#define _CRWWP_UNIVERSAL_H_

#include <atomic>
#include <stdexcept>
#include <cstdint>
#include <functional>
#include <cassert>
#include <mutex>

#include "../common/RIStaticPerThread.hpp"

/**
 * <h1> Universal C-RW-WP </h1>
 *
 * This is a blocking universal construct that protects the object or data
 * structure with a single C-RW-WP.
 *
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename C, typename R = bool>
class CRWWPUniversal {

private:
    class TicketLock {
        alignas(128) std::atomic<uint64_t> ticket {0};
        alignas(128) std::atomic<uint64_t> grant {0};
    public:
        bool isLocked() { return grant.load(std::memory_order_acquire) != ticket.load(std::memory_order_acquire); }
        void lock() {
            auto tkt = ticket.fetch_add(1);
            while (tkt != grant.load(std::memory_order_acquire)) std::this_thread::yield();
        }
        void unlock() {
            auto tkt = grant.load(std::memory_order_relaxed);
            grant.store(tkt+1, std::memory_order_release);
        }
    };


    static const int MAX_THREADS = 128;
    //static const int LOCKED = 1;
    //static const int UNLOCKED = 0;
    const int maxThreads;
    RIStaticPerThread ri { MAX_THREADS };
    //alignas(128) std::atomic<int> cohort { UNLOCKED };
    TicketLock cohort;
    alignas(128) C* instance;


public:
    CRWWPUniversal(C* inst, const int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
        instance = inst;
    }


    ~CRWWPUniversal() {
        delete instance;
    }


    R applyUpdate(std::function<R(C*)>& mutativeFunc, const int tid) {
        cohort.lock();
        while (!ri.isEmpty()) std::this_thread::yield();
        R result = mutativeFunc(instance);
        cohort.unlock();
        return result;
    }


    R applyRead(std::function<R(C*)>& readFunc, const int tid) {
        // lock()
        while (true) {
            ri.arrive(tid);
            if (!cohort.isLocked()) break;
            ri.depart(tid);
            while (cohort.isLocked()) std::this_thread::yield();
        }
        R result = readFunc(instance);
        // unlock()
        ri.depart(tid);
        return result;
    }

};


// This class can be used to simplify the usage of sets/multisets with CXMutation.
// For generic code you don't need it (and can't use it), but it can serve as an example of how to use lambdas.
// C must be a set/multiset where the keys are of type CKey
template<typename C, typename CKey>
class CRWWPSet {
private:
    static const int MAX_THREADS = 128;
    const int maxThreads;
    CRWWPUniversal<C> crwwp{new C(), maxThreads};

public:
    CRWWPSet(const int maxThreads=MAX_THREADS) : maxThreads{maxThreads} { }

    static std::string className() { return "CRWWP-" + C::className(); }

    // Progress-condition: blocking
    bool add(CKey* key, const int tid) {
        std::function<bool(C*)> addFunc = [key] (C* set) { return set->add(key); };
        return crwwp.applyUpdate(addFunc, tid);
    }

    // Progress-condition: blocking
    bool remove(CKey* key, const int tid) {
        std::function<bool(C*)> removeFunc = [key] (C* set) { return set->remove(key); };
        return crwwp.applyUpdate(removeFunc, tid);
    }

    // Progress-condition: blocking
    bool contains(CKey* key, const int tid) {
        std::function<bool(C*)> containsFunc = [key] (C* set) { return set->contains(key); };
        return crwwp.applyRead(containsFunc, tid);
    }

    bool iterateAll(std::function<bool(CKey*)> itfun, const int tid) {
        std::function<bool(C*)> func = [&itfun] (C* set) {
        	return set->iterateAll(itfun);
        };
        return crwwp.applyRead(func, tid);
    }

    // Progress-condition: blocking
    void addAll(CKey** keys, const int size, const int tid) {
        std::function<bool(C*)> addFunc = [keys,size] (C* set) {
            for (int i = 0; i < size; i++) set->add(keys[i]);
            return true;
        };
        crwwp.applyUpdate(addFunc, tid);
    }
};

#endif /* _CRWWP_UNIVERSAL_H_ */

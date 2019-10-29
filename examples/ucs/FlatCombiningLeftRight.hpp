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

#ifndef _FLAT_COMBINING_LEFT_RIGHT_H_
#define _FLAT_COMBINING_LEFT_RIGHT_H_

#include <atomic>
#include <stdexcept>
#include <cstdint>
#include <functional>
#include <cassert>
#include <mutex>
#include <thread>

#include "../common/RIStaticPerThread.hpp"

/**
 * <h1> Universal Construct with Left-Right with Flat Combining </h1>
 *
 * Uses the Left-Right technique by Correia and Ramalhe and
 * adds Flat Combining to it:
 * The writersMutex is a simple spin lock because the Flat Combining technique
 * provides starvation-freedom amongst writers.
 *
 * There is a difference here from using C-RW-WP with Flat Combining: because
 * we apply the same mutation twice, we have to make a local copy of the
 * flat combining array in applyMutation() before starting to apply the
 * mutation functions, otherwise it could happen that in the middle of
 * toggleVersionAndWait() a new writer would come and we would apply its
 * mutation on the second instance even though we didn't apply it on the
 * first instance. This would be incorrect.
 *
 * Memory Reclamation:           None needed
 * Progress for applyMutation(): Blocking (starvation-free)
 * Progress for applyRead():     Wait-Free (population-oblivious)
 *
 * Left-Right paper: https://github.com/pramalhe/ConcurrencyFreaks/blob/master/papers/left-right-2014.pdf
 * Flat Combining paper:  http://dl.acm.org/citation.cfm?id=1810540
 *
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename C, typename R = bool>
class FlatCombiningLeftRight {

private:
    static const int CLPAD = 128/sizeof(uintptr_t);
    static const int MAX_THREADS = 128;
    static const int LOCKED = 1;
    static const int UNLOCKED = 0;
    const int maxThreads;
    // Stuff use by the Flat Combining mechanism
    alignas(128) std::atomic< std::function<R(C*)>* >* fc;
    alignas(128) R* results;
    // Stuff used by the Left-Right mechanism
    alignas(128) std::atomic<int> writersMutex { UNLOCKED };
    alignas(128) std::atomic<int> leftRight { 0 };
    alignas(128) std::atomic<int> versionIndex { 0 };
    RIStaticPerThread ri[2] { MAX_THREADS, MAX_THREADS };
    alignas(128) C* inst[2];


    // This is equivalent to rcu_synchronize()
    void toggleVersionAndWait() {
        const int localVI = versionIndex.load();
        const int prevVI = localVI & 0x1;
        const int nextVI = (localVI+1) & 0x1;
        // Wait for Readers from next version
        while (!ri[nextVI].isEmpty()) ; // spin
        // Toggle the versionIndex variable
        versionIndex.store(nextVI);
        // Wait for Readers from previous version
        while (!ri[prevVI].isEmpty()) ; // spin
    }

public:
    FlatCombiningLeftRight(C* instance, const int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
        inst[0] = instance;
        inst[1] = new C(*instance); // Make a copy for the second instance
        fc = new std::atomic< std::function<R(C*)>* >[maxThreads*CLPAD];
        results = new R[maxThreads*CLPAD];
        for (int i = 0; i < maxThreads; i++) {
            fc[i*CLPAD].store(nullptr, std::memory_order_relaxed);
        }
    }


    ~FlatCombiningLeftRight() {
        delete inst[0];
        delete inst[1];
        delete[] fc;
        delete[] results;
    }


    static std::string className() { return "FlatCombiningLeftRight-"; }


    // Progress: Blocking (starvation-free)
    R applyUpdate(std::function<R(C*)>& mutativeFunc, const int tid) {
        // Add our mutation to the array of flat combining
        fc[tid*CLPAD].store(&mutativeFunc);
        // Lock writersMutex
        while (true) {
            int unlocked = UNLOCKED;
            if (writersMutex.load() == UNLOCKED &&
                writersMutex.compare_exchange_strong(unlocked, LOCKED)) break;
            // Check if another thread executed my mutation
            if (fc[tid*CLPAD].load(std::memory_order_acquire) == nullptr) {
                return results[tid*CLPAD];
            }
            std::this_thread::yield();
        }
        // Save a local copy of the flat combining array
        std::function<R(C*)>* lfc[maxThreads];
        for (int i = 0; i < maxThreads; i++) {
            lfc[i] = fc[i*CLPAD].load(std::memory_order_acquire);
        }
        // For each mutation in the local flat combining array, apply it in
        // the order of the array and save the result.
        const int prevLR = leftRight.load();
        const int nextLR = (prevLR+1)&1;
        for (int i = 0; i < maxThreads; i++) {
            auto mutation = lfc[i];
            if (mutation == nullptr) continue;
            results[i*CLPAD] = (*mutation)(inst[nextLR]);
        }
        leftRight.store(nextLR);
        toggleVersionAndWait();
        // This time, set the entry in the flat combining array to nullptr
        for (int i = 0; i < maxThreads; i++) {
            auto mutation = lfc[i];
            if (mutation == nullptr) continue;
            (*mutation)(inst[prevLR]);
            fc[i*CLPAD].store(nullptr, std::memory_order_release);
        }
        // unlock()
        writersMutex.store(UNLOCKED, std::memory_order_release);
        return results[tid*CLPAD];
    }


    // Progress: Wait-Free Population Oblivious
    R applyRead(std::function<R(C*)>& readFunc, const int tid) {
        const int localVI = versionIndex.load();
        ri[localVI].arrive(tid);
        R result = readFunc(inst[leftRight.load()]);
        ri[localVI].depart(tid);
        return result;
    }

};

#endif /* _FLAT_COMBINING_LEFT_RIGHT_H_ */

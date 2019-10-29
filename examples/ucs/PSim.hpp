/*
 * Copyright 2014-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *
 * This work is published under the MIT license. See LICENSE.TXT
 */
#ifndef _PSIM_UNIVERSAL_WF_H_
#define _PSIM_UNIVERSAL_WF_H_

#include <atomic>
#include <stdexcept>
#include <cstdint>
#include <functional>
#include <cassert>

#include "../common/HazardPointers.hpp"

using namespace std::chrono;

/**
 * <h1> P-Sim universal wait-free construct </h1>
 *
 * A Universal wait-free construction for concurrent objects by
 * Panagiota Fatourou and Nikolaos Kallimanis:
 * http://thalis.cs.uoi.gr/tech_reports/publications/TR2011-01.pdf
 *
 * (very) loosely based on the original code by Nikolaos Kallimanis:
 * https://github.com/nkallima/sim-universal-construction/blob/master/sim-synch1.4/sim.c
 *
 * Things that we have changed in this optimized version from the original implementation:
 * - It's in portable C++, using C++ atomics and its memory model;
 * - This implementation is portable across different CPUs and systems, i.e.
 *   unlike the original implementation which was meant for TSO, this one
 *   can be used in PowerPC, ARM, whatever has a C++14 compiler;
 * - We've used relaxed atomics where possible;
 * - The members of ObjectState must be std::atomic<> because they might be
 *   read by a thread while another thread is writting them, which would be a
 *   race by definition, and therefore, be undefined behavior. We use relaxed
 *   atomics which in practice has no difference in throughput;
 * - We don't use fetch-and-add for the arrays of bits because it would not be
 *   wait-free on architectures other than x86;
 * - The apply_op() method is named applyMutation();
 * - We've added Hazard Pointers for memory reclamation, and in a wait-free way;
 * - For improved throughput, we use one HP instance for the mutations and
 *   another HP instance for the data structure instance;
 * - Instead of function pointers we use std::function<> which allows to pass
 *   lambdas with argument capture. This means we don't need to explicitly
 *   store the arguments of the function, and that different functions can be
 *   called, for either applyRead() or applyMutation(). The only constraint
 *   is that the return value must be R for all of those functions, and that
 *   R must be a type that fits in a std::atomic<>, i.e. 64 bits at most;
 * - The pool of ObjectState has only two instances per thread, while the
 *   original had 16;
 * - We don't use an HalfObjectState (no need for it);
 * - We don't have a backoff mechanism;
 *
 */
template<typename C, typename R = bool>  // R must fit in an a std::atomic<R>
class PSim {

private:
    static const int MAX_THREADS = 128; // Increase this for the stress tests

    class ObjectState {
    public:
        std::atomic<bool>  applied[MAX_THREADS];
        std::atomic<R>     results[MAX_THREADS];
        std::atomic<C*>    instance;
        ObjectState(C* firstInstance=nullptr) { // Used only by the first objState instance
            for (int i = 0; i < MAX_THREADS; i++) applied[i].store(false, std::memory_order_relaxed);
            for (int i = 0; i < MAX_THREADS; i++) results[i].store(R{}, std::memory_order_relaxed);
            instance.store(firstInstance, std::memory_order_relaxed);
        }
        ~ObjectState() {
            delete instance.load();
        }
        // We can't use the "copy assignment operator" because we need to make sure that the instance
        // we're copying is the one we have protected with the hazard pointer
        void copyFrom(const ObjectState& from, C* inst) {
            for (int i = 0; i < MAX_THREADS; i++) applied[i].store(from.applied[i].load(), std::memory_order_relaxed);
            for (int i = 0; i < MAX_THREADS; i++) results[i].store(from.results[i].load(), std::memory_order_relaxed);
            instance.store(new C(*inst), std::memory_order_release);
        }
    };

    typedef union {
        struct {
            uint64_t seq  : 48;
            uint64_t index: 16;
        } u;            // struct_data
        uint64_t raw;   // raw_data
    } SeqPointer;

    const int maxThreads;

    // The array of mutations must be atomic due to read-write races, but it's all relaxed
    alignas(128) std::atomic<std::function<R(C*)>*>  mutations[MAX_THREADS]; // Array of mutations
    alignas(128) std::atomic<bool>                   announce[MAX_THREADS];
    alignas(128) ObjectState                         objStates[MAX_THREADS*2]; // We need two ObjecStates per thread
    alignas(128) std::atomic<SeqPointer>             objPointer; // Points to an ObjectState in objStates array

    // Hazard Pointers
    HazardPointers<std::function<R(C*)>> hpMut {1, maxThreads};
    const int kHpMut = 0;
    HazardPointers<C> hpInst {1, maxThreads};
    const int kHpInst = 0;


public:

    PSim(C* inst, const int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
        SeqPointer first;
        first.u.index = 0; // Point to objStates[0]
        first.u.seq = 0;
        objPointer.store(first, std::memory_order_relaxed);
        objStates[0].instance.store(inst, std::memory_order_relaxed);
        for (int i = 0; i < maxThreads; i++) {
            mutations[i].store(nullptr, std::memory_order_relaxed);
            announce[i].store(false, std::memory_order_relaxed);
        }
    }


    ~PSim() {
        for (int i = 0; i < maxThreads; i++) {
            if (mutations[i].load() != nullptr) delete mutations[i];
        }
    }


    static std::string className() { return "PSim-"; }


    R applyUpdate(std::function<R(C*)>& mutativeFunc, const int tid) {
        // Publish mutation and retire previous mutation
        auto oldmut = mutations[tid].load(std::memory_order_relaxed);
        std::function<bool(C*)>* newmut = new std::function<bool(C*)>(mutativeFunc);
        mutations[tid].store(newmut, std::memory_order_relaxed);
        if (oldmut != nullptr) hpMut.retire(oldmut, tid);
        const bool newrequest = !announce[tid].load();
        announce[tid].store(newrequest);
        std::this_thread::sleep_for(1us); // backoff like CX
        for (int iter = 0; iter < 2; iter++) {
            SeqPointer lptr = objPointer.load();
            // Protect the instance with an HP
            C* inst = hpInst.protectPtr(kHpInst, objStates[lptr.u.index].instance.load(), tid);
            if (lptr.raw != objPointer.load().raw) continue;
            // Get a new ObjectState instance from the pool on which to apply the mutations
            const int myIndex = (lptr.u.index == 2*tid) ? 2*tid+1 : 2*tid ;
            ObjectState& newState = objStates[myIndex];
            // Check if the previous inst needs to be cleaned
            C* delInst = newState.instance.load();
            if (delInst != nullptr) hpInst.retire(delInst, tid);
            // Copy the contents of the current ObjectState into the new ObjectState, except for inst
            newState.copyFrom(objStates[lptr.u.index], inst);
            // Save a pointer to the copy of the instance because that's where we're applying the mutations
            C* newInst = newState.instance.load();
            if (lptr.raw != objPointer.load().raw) continue;
            // Check if my mutation has been applied
            if (newState.applied[tid] == newrequest) break;
            // Help other requests, starting from zero
            for (int i = 0; i < maxThreads; i++) {
                // Check if it is an open request
                if (announce[i].load() == newState.applied[i]) continue;
                newState.applied[i] = !newState.applied[i];
                // Apply the mutation and save the result
                auto mutation = hpMut.protectPtr(kHpMut, mutations[i].load(), tid);
                if (mutation != mutations[i].load()) continue;
                newState.results[i].store((*mutation)(newInst), std::memory_order_relaxed);
                if (lptr.raw != objPointer.load().raw) break;
            }
            if (lptr.raw != objPointer.load().raw) continue;
            // Try to make all the mutations visible
            SeqPointer newptr;
            newptr.u.index = myIndex;
            newptr.u.seq = lptr.u.seq+1;
            if (objPointer.compare_exchange_strong(lptr, newptr)) break;
        }
        hpInst.clear(tid);
        hpMut.clear(tid);
        SeqPointer lptr = objPointer.load();
        return objStates[lptr.u.index].results[tid].load();
    }


    inline R applyRead(std::function<R(C*)>& mutativeFunc, const int tid) {
        return applyUpdate(mutativeFunc,tid);
    }
};

#endif /* _PSIM_UNIVERSAL_WF_H_ */

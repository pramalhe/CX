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

#ifndef _STRONG_TRY_READ_INDICATOR_READER_WRITER_LOCK_H_
#define _STRONG_TRY_READ_INDICATOR_READER_WRITER_LOCK_H_

#include <atomic>
#include <string>
#include <thread>

/**
 * <h1> Try-Lock Reader-Preference with Intermediate states - TryRWLockFRThreeState</h1>
 *
 * This RW-Lock is specifically designed to not have spurious failures when
 * doing trylock() for either the readlock or the writelock.
 * This means it can be used as part of a higher level synchronization mechanisms,
 * like the CX.
 * When using a good ReadIndicator (like RIStaticPerThread or an array of counters)
 * this lock has excellent scalability for readers.
 *
 * writerState can have four different states:
 * - NOLOCK: There is no writer trying to acquire the lock;
 * - HLOCK:  At least one writer is attempting to acquire the lock;
 * - RLOCK:  An intermidate state where a writer thread is giving up on exclusive lock, only allowing readers to acquire the lock;
 * - WLOCK:  The writer thread is holding the lock in exclusive mode;
 *
 * The transitions for writerState are:
 * - seq || NOLOCK -> seq+1 || HLOCK:   A writer is attempting to acquire in exclusive mode (CAS);
 * - seq || HLOCK -> seq || WLOCK:      A writer is attempting to acquire in exclusive mode (CAS);
 * - seq || HLOCK -> seq || NOLOCK:     A reader won the lock (CAS);
 * - seq || WLOCK -> seq || RLOCK:      A writer is unlocking but only allowing readers to enter;
 * - seq || RLOCK -> seq || NOLOCK:     A writer has unlocked and now allows both readers and writers to acquire the lock
 *
 *
 * @author Andreia Correia
 * @author Pedro Ramalhete
 */
class StrongTryRIRWLock {

private:
    // States
    static const uint64_t NOLOCK = 0;
    static const uint64_t HLOCK = 1;
    static const uint64_t RLOCK = 2;
    static const uint64_t WLOCK = 3;

    struct StructData {
        uint64_t seq : 62;
        uint64_t state: 2;

        bool operator != (const StructData& other) const {
            return seq != other.seq || state != other.state;
        }
    };


    // Customized ReadIndicator
    class RIStaticPerThread {

    private:
        const int maxThreads;
        alignas(128) std::atomic<uint64_t>* states;

        static const uint64_t NOT_READING = 0;
        static const uint64_t READING = 1;
        static const int CLPAD = 128/sizeof(uint64_t);

    public:
        RIStaticPerThread(int maxThreads) : maxThreads{maxThreads} {
            states = new std::atomic<uint64_t>[maxThreads*CLPAD];
            for (int tid = 0; tid < maxThreads; tid++) {
                states[tid*CLPAD].store(NOT_READING, std::memory_order_relaxed);
            }
        }

        ~RIStaticPerThread() {
            delete[] states;
        }

        // Will attempt to pass all current READING states to
        inline void abortRollback() noexcept {
            for (int tid = 0; tid < maxThreads; tid++) {
                if (states[tid*CLPAD].load() != READING) continue;
                uint64_t read = READING;
                states[tid*CLPAD].compare_exchange_strong(read, READING+1);
            }
        }

        // Returns true if the arrival was successfully rollbacked.
        // If there was a writer changing the state to READING+1 then it will
        // return false, meaning that the arrive() is still valid and visible.
        inline bool rollbackArrive(const int tid) noexcept {
            return (states[tid*CLPAD].fetch_add(-1) == READING);
        }

        inline void arrive(const int tid) noexcept {
            states[tid*CLPAD].store(READING);
        }

        inline void depart(const int tid) noexcept {
            states[tid*CLPAD].store(NOT_READING); // Making this "memory_order_release" will cause overflows!
        }

        inline bool isEmpty() noexcept {
            for (int tid = 0; tid < maxThreads; tid++) {
                if (states[tid*CLPAD].load() != NOT_READING) return false;
            }
            return true;
        }
    };


    const int maxThreads;

    // ReadIndicator
    RIStaticPerThread ri {maxThreads};

    alignas(128) std::atomic<StructData> wstate {{0,NOLOCK}};
public:
    /**
     * Default constructor
     */
    StrongTryRIRWLock(int maxThreads) : maxThreads{maxThreads} {
    }

    ~StrongTryRIRWLock() {
    }

    static std::string className() { return "StrongTryRWLock"; }

    inline bool sharedTryLock(const int tid) noexcept {
        if (wstate.load().state == WLOCK) return false; // There is a writer
        ri.arrive(tid);
        StructData ws = wstate.load();
        if (ws.state == HLOCK) {
            if (wstate.compare_exchange_strong(ws, {ws.seq,NOLOCK})) return true;
            ws = wstate.load();
        }
        return (ws.state != WLOCK || !ri.rollbackArrive(tid));
    }

    inline void sharedLock(const int tid) noexcept {
        while (!sharedTryLock(tid)) std::this_thread::yield();
    }

    inline void sharedUnlock(const int tid) noexcept {
        ri.depart(tid);
    }


    inline bool exclusiveTryLock(const int tid) noexcept {
        StructData ws = wstate.load();
        if (ws.state == WLOCK || ws.state == RLOCK) return false;
        if (!ri.isEmpty()) return false;
        if (ws.state == HLOCK) {
            if(ws!=wstate.load()) return false; // possible opt
            return wstate.compare_exchange_strong(ws, {ws.seq, WLOCK}); // A writer jumped ahead of me
        }
        StructData next = {ws.seq+1,HLOCK};
        wstate.compare_exchange_strong(ws, next);
        if (!ri.isEmpty()) return false;
        if (wstate.load()!=next) return false;
        return wstate.compare_exchange_strong(next, {next.seq,WLOCK});
    }


    inline void exclusiveLock(const int tid) noexcept {
        while (!exclusiveTryLock(tid)) std::this_thread::yield();
    }

    inline void exclusiveUnlock() noexcept {
        StructData ws = wstate.load(std::memory_order_relaxed);
        wstate.store({ws.seq, RLOCK});
        ri.abortRollback();
        wstate.store({ws.seq, NOLOCK});
    }

    inline void setReadLock() noexcept {
        StructData ws = wstate.load(std::memory_order_relaxed);
        wstate.store({ws.seq, RLOCK});
    }

    inline void setReadUnlock() noexcept {
        StructData ws = wstate.load(std::memory_order_relaxed);
        wstate.store({ws.seq, NOLOCK});
    }

    // This is the "generic" downgrade
    inline void downgrade() noexcept {
        StructData ws = wstate.load(std::memory_order_relaxed);
        wstate.store({ws.seq, RLOCK});
        ri.abortRollback();
    }
};

#endif /* _STRONG_TRY_READ_INDICATOR_READER_WRITER_LOCK_H_ */

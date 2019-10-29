/******************************************************************************
 * Copyright (c) 2016-2018, Pedro Ramalhete, Andreia Correia
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
#ifndef _BENCHMARK_LATENCY_SETS_H_
#define _BENCHMARK_LATENCY_SETS_H_

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>


// Regular UserData
struct UserData  {
    long long seq;
    int tid;
    UserData(long long lseq, int ltid=0) {
        this->seq = lseq;
        this->tid = ltid;
    }
    UserData() {
        this->seq = -2;
        this->tid = -2;
    }
    UserData(const UserData &other) : seq(other.seq), tid(other.tid) { }

    bool operator < (const UserData& other) const {
        return seq < other.seq;
    }
    bool operator == (const UserData& other) const {
        return seq == other.seq && tid == other.tid;
    }
    bool operator != (const UserData& other) const {
        return seq != other.seq || tid != other.tid;
    }
};

/**
 * This is a micro-benchmark for Latency tests
 */
template<typename K = UserData>
class BenchmarkLatencySets {

private:
    struct UserData  {
        long long seq;
        int tid;
        UserData(long long lseq, int ltid) {
            this->seq = lseq;
            this->tid = ltid;
        }
        UserData() {
            this->seq = -2;
            this->tid = -2;
        }
        UserData(const UserData &other) : seq(other.seq), tid(other.tid) { }

        bool operator < (const UserData& other) const {
            return seq < other.seq;
        }
    };

    struct Result {
        nanoseconds nsEnq = 0ns;
        nanoseconds nsDeq = 0ns;
        long long numEnq = 0;
        long long numDeq = 0;
        long long totOpsSec = 0;

        Result() { }

        Result(const Result &other) {
            nsEnq = other.nsEnq;
            nsDeq = other.nsDeq;
            numEnq = other.numEnq;
            numDeq = other.numDeq;
            totOpsSec = other.totOpsSec;
        }

        bool operator < (const Result& other) const {
            return totOpsSec < other.totOpsSec;
        }
    };

    // Latency constants
    static const long long kLatencyMeasures =     200*1000*1000LL;   // We measure 200M add()/remove() divided among the different threads
    static const long long kLatencyWarmupIterations = 1000*1000LL;   // At start of latency tests we do 1M warmup add()/remove()
    static const long long NSEC_IN_SEC = 1000000000LL;

    int numThreads;

public:
    BenchmarkLatencySets(int numThreads) {
        this->numThreads = numThreads;
    }




    /*
     * Execute latency benchmarks
     * Make sure to enable high priority for the process
     *
     * We can use this Mathematica function to compute the Inverse CDF of a Poisson and model the latency at 99.99% for lock-free algorithms:
     * https://reference.wolfram.com/language/ref/InverseCDF.html
     *
     * We only do one run for this benchmark
     *
     * The scenario is 100% write operations (half add, half remove)
     */
    template<typename S>
    int latency(std::string& className, const int numElements) {
        atomic<bool> start = { false };
        S* set = new S(numThreads);

        // Create all the keys in the concurrent set
        K** udarray = new K*[numElements];
        for (int i = 0; i < numElements; i++) udarray[i] = new K(i);

        auto latency_lambda = [this,&start,&set,&udarray,numElements](nanoseconds* delays, const int tid) {
            long long delayIndex = 0;
            while (!start.load()) ; // spin
            uint64_t seed = tid+1234567890123456781ULL;
            // Warmup + Measurements
            for (int iter=0; iter < kLatencyMeasures/numThreads+kLatencyWarmupIterations; iter++) {
                seed = randomLong(seed);
                auto ix = (unsigned int)(seed%numElements);
                auto startBeats = steady_clock::now();
                if (set->remove(*udarray[ix], tid)) set->add(*udarray[ix], tid);
                auto stopBeats = steady_clock::now();
                if (iter >= kLatencyWarmupIterations) delays[delayIndex++] = (stopBeats-startBeats);
            }
        };

        nanoseconds* delays[numThreads];
        for (int it = 0; it < numThreads; it++) {
            delays[it] = new nanoseconds[kLatencyMeasures/numThreads];
            for (int imeas=0; imeas < kLatencyMeasures/numThreads; imeas++) delays[it][imeas] = 0ns;
        }

        className = S::className();
        std::cout << "##### " << S::className() << " #####  \n";
        // Add all the items to the list
        set->addAll(udarray, numElements, 0);
        thread latencyThreads[numThreads];
        for (int tid = 0; tid < numThreads; tid++) latencyThreads[tid] = thread(latency_lambda, delays[tid], tid);
        this_thread::sleep_for(100ms);
        start.store(true);
        for (int tid = 0; tid < numThreads; tid++) latencyThreads[tid].join();
        delete set;

        // Aggregate all the delays for enqueues and dequeues and compute the maxs
        cout << "Aggregating delays for " << kLatencyMeasures/1000000 << " million measurements...\n";
        vector<nanoseconds> aggDelay(kLatencyMeasures);
        long long idx = 0;
        for (int it = 0; it < numThreads; it++) {
            for (int i = 0; i < kLatencyMeasures/numThreads; i++) {
                aggDelay[idx] = delays[it][i];
                idx++;
            }
        }

        // Sort the aggregated delays
        cout << "Sorting delays...\n";
        sort(aggDelay.begin(), aggDelay.end());

        // Show the 50% (median), 90%, 99%, 99.9%, 99.99%, 99.999% and maximum in microsecond/nanoseconds units
        long per50000 = (long)(kLatencyMeasures*50000LL/100000LL);
        long per90000 = (long)(kLatencyMeasures*90000LL/100000LL);
        long per99000 = (long)(kLatencyMeasures*99000LL/100000LL);
        long per99900 = (long)(kLatencyMeasures*99900LL/100000LL);
        long per99990 = (long)(kLatencyMeasures*99990LL/100000LL);
        long per99999 = (long)(kLatencyMeasures*99999LL/100000LL);
        long imax = kLatencyMeasures-1;

        cout << "Delay (us): 50%=" << aggDelay[per50000].count()/1000
             << "  90%=" <<     aggDelay[per90000].count()/1000 << "  99%="    << aggDelay[per99000].count()/1000
             << "  99.9%=" <<   aggDelay[per99900].count()/1000 << "  99.99%=" << aggDelay[per99990].count()/1000
             << "  99.999%=" << aggDelay[per99999].count()/1000 << "  max="    << aggDelay[imax].count()/1000 << "\n";

        // Show in csv format
        cout << "Enqueue delay (us):\n";
        cout << "50, " << aggDelay[per50000].count()/1000 << "\n";
        cout << "90, " << aggDelay[per90000].count()/1000 << "\n";
        cout << "99, " << aggDelay[per99000].count()/1000 << "\n";
        cout << "99.9, " << aggDelay[per99900].count()/1000 << "\n";
        cout << "99.99, " << aggDelay[per99990].count()/1000 << "\n";
        cout << "99.999, " << aggDelay[per99999].count()/1000 << "\n";

        // Cleanup
        for (int it = 0; it < numThreads; it++) delete[] delays[it];
        return 0;
    }

    /**
     * An imprecise but fast random number generator
     */
    uint64_t randomLong(uint64_t x) {
        x ^= x >> 12; // a
        x ^= x << 25; // b
        x ^= x >> 27; // c
        return x * 2685821657736338717LL;
    }

};

#endif

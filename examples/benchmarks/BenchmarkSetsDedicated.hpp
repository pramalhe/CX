#ifndef _BENCHMARK_SETS_DEDICATED_H_
#define _BENCHMARK_SETS_DEDICATED_H_

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

using namespace std;
using namespace chrono;

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


namespace std {
    template <>
    struct hash<UserData> {
        std::size_t operator()(const UserData& k) const {
            using std::size_t;
            using std::hash;
            return (hash<long long>()(k.seq));  // This hash has no collisions, which is irealistic
        }
    };
}

struct TwoResults {
    long long readops {0};
    long long updateops {0};
    bool operator < (const TwoResults& other) const {
        if (readops != 0) return readops < other.readops;
        else return updateops < other.updateops;
    }
};

/**
 * This is a micro-benchmark of sets, with dedicated threads, used in the CX paper
 */
class BenchmarkSetsDedicated {

private:
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

    static const long long NSEC_IN_SEC = 1000000000LL;

    int numThreads;

public:
    BenchmarkSetsDedicated(int numThreads) {
        this->numThreads = numThreads;
    }


    /**
     * When doing "updates" we execute a random removal and if the removal is successful we do an add() of the
     * same item immediately after. This keeps the size of the data structure equal to the original size (minus
     * MAX_THREADS items at most) which gives more deterministic results.
     */
    template<typename S, typename K>
    TwoResults benchmark(std::string& className, const seconds testLengthSeconds, const int numRuns, const int numElements) {
        TwoResults ops[numThreads][numRuns];
        long long lengthSec[numRuns];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };
        S* set = nullptr;
        set = new S(numThreads);

        // Create all the keys in the concurrent set
        K** udarray = new K*[numElements];
        for (int i = 0; i < numElements; i++) udarray[i] = new K(i);
        uint64_t itersize = (numElements<1000)? numElements : 1000;
        // Can either be a Reader or a Writer
        auto rw_lambda = [this,&quit,&startFlag,&set,&udarray,&numElements,&itersize](TwoResults *ops, const int tid) {
            const bool isReader = (tid%2 == 0); // Threads with even tids are readers. Odd tids are updaters
            TwoResults numOps {};
            while (!startFlag.load()) ; // spin
            uint64_t seed = tid+1234567890123456781ULL;
            while (!quit.load()) {
            	seed = randomLong(seed);
            	auto ix = (uint64_t)(seed%numElements);
                if (isReader) {
                    set->iterate([](K* k) { return (k->seq+1 > 0); }, tid, itersize, *udarray[ix]);
                    numOps.readops++;
                } else {
                    if (set->remove(*udarray[ix], tid)) {
                        numOps.updateops++;
                        set->add(*udarray[ix], tid);
                    }
                    numOps.updateops++;
                }
            }
            *ops = numOps;
        };

        for (int irun = 0; irun < numRuns; irun++) {
            // Add all the items to the list
            set->addAll(udarray, numElements, 0);
            if (irun == 0) {
                className = set->className();
                std::cout << "##### " << set->className() << " #####  \n";
            }
            thread rwThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) rwThreads[tid] = thread(rw_lambda, &ops[tid][irun], tid);
            this_thread::sleep_for(100ms);
            auto startBeats = steady_clock::now();
            startFlag.store(true);
            // Sleep for testLengthSeconds seconds
            this_thread::sleep_for(testLengthSeconds);
            quit.store(true);
            auto stopBeats = steady_clock::now();
            for (int tid = 0; tid < numThreads; tid++) rwThreads[tid].join();
            lengthSec[irun] = (stopBeats-startBeats).count();
            quit.store(false);
            startFlag.store(false);
            // Measure the time the destructor takes to complete and if it's more than 1 second, print it out
            auto startDel = steady_clock::now();
            delete set;
            auto stopDel = steady_clock::now();
            if ((startDel-stopDel).count() > NSEC_IN_SEC) {
                std::cout << "Destructor took " << (startDel-stopDel).count()/NSEC_IN_SEC << " seconds\n";
            }
            // Compute ops at the end of each run
            TwoResults agg {};
            for (int tid = 0; tid < numThreads; tid++) {
                agg.readops += ops[tid][irun].readops*1000000000LL/lengthSec[irun];
                agg.updateops += ops[tid][irun].updateops*1000000000LL/lengthSec[irun];
            }
        }

        for (int i = 0; i < numElements; i++) delete udarray[i];
        delete[] udarray;

        // Accounting
        vector<TwoResults> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun].readops += ops[tid][irun].readops*1000000000LL/lengthSec[irun];
                agg[irun].updateops += ops[tid][irun].updateops*1000000000LL/lengthSec[irun];
            }
        }

        // Compute the median. numRuns must be an odd number
        sort(agg.begin(),agg.end());
        TwoResults medianops = agg[numRuns/2];
        std::cout << "Read Ops/sec = " << medianops.readops << "     Update Ops/sec = " << medianops.updateops << "\n";
        return medianops;
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

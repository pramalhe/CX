
/*
 * Executes the following non-blocking (linked list based) queues in a single-enqueue-single-dequeue benchmark:
 * - Michael-Scott (lock-free)
 * - SimQueue (wait-free bounded)
 * - Turn Queue (wait-free bounded)
 * - MWC-LF (lock-free)
 * - MWC-WF (wait-free bounded)
 */
#include <iostream>
#include <fstream>
#include <cstring>

#include "datastructures/lockfree/MichaelScottQueue.hpp"
#include "datastructures/waitfree/SimQueue.hpp"
#include "datastructures/waitfree/TurnQueue.hpp"
#include "common/UCQueue.hpp"
#include "datastructures/sequential/LinkedListQueue.hpp"
#include "ucs/FlatCombiningCRWWP.hpp"
#include "ucs/FlatCombiningLeftRight.hpp"
#include "ucs/PSimOpt.hpp"
#include "ucs/CXMutationWF.hpp"
#include "benchmarks/BenchmarkQueues.hpp"


#define MILLION  1000000LL

int main(void) {
    const std::string dataFilename {"data/q-ll-enq-deq.txt"};
    vector<int> threadList = { 1, 2, 4, 8 };                 // For the laptop
    //vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64, 72, 80, 88, 96 }; // For Cervino
    const int numRuns = 1;                                   // Number of runs
    const long numPairs = 10*MILLION;                        // 10M is fast enough on the laptop, but on cervino we can use 100M
    const int EMAX_CLASS = 6;
    uint64_t results[EMAX_CLASS][threadList.size()];
    std::string cNames[EMAX_CLASS];
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*threadList.size());

    // Enq-Deq Throughput benchmarks
    for (int ithread = 0; ithread < threadList.size(); ithread++) {
        int nThreads = threadList[ithread];
        int iclass = 0;
        BenchmarkQueues bench(nThreads);
        std::cout << "\n----- q-ll-enq-deq   threads=" << nThreads << "   pairs=" << numPairs/MILLION << "M   runs=" << numRuns << "-----\n";
        results[iclass++][ithread] = bench.enqDeq<MichaelScottQueue<UserData>>                                                                  (cNames[iclass], numPairs, numRuns);
        results[iclass++][ithread] = bench.enqDeq<SimQueue<UserData>>                                                                           (cNames[iclass], numPairs, numRuns);
        results[iclass++][ithread] = bench.enqDeq<TurnQueue<UserData>>                                                                          (cNames[iclass], numPairs, numRuns);
        results[iclass++][ithread] = bench.enqDeq<UCQueue<FlatCombiningCRWWP<LinkedListQueue<UserData>>,LinkedListQueue<UserData>,UserData>>    (cNames[iclass], numPairs, numRuns);
        results[iclass++][ithread] = bench.enqDeq<UCQueue<FlatCombiningLeftRight<LinkedListQueue<UserData>>,LinkedListQueue<UserData>,UserData>>(cNames[iclass], numPairs, numRuns);
        results[iclass++][ithread] = bench.enqDeq<UCQueue<CXMutationWF<LinkedListQueue<UserData>>,LinkedListQueue<UserData>,UserData>>          (cNames[iclass], numPairs, numRuns);
        // PSim+LinkedListQueue is just too slow to measure
    }

    // Export tab-separated values to a file to be imported in gnuplot or excel
    ofstream dataFile;
    dataFile.open(dataFilename);
    dataFile << "Threads\t";
    // Printf class names for each column
    for (int iclass = 0; iclass < EMAX_CLASS; iclass++) dataFile << cNames[iclass] << "\t";
    dataFile << "\n";
    for (int ithread = 0; ithread < threadList.size(); ithread++) {
        dataFile << threadList[ithread] << "\t";
        for (int iclass = 0; iclass < EMAX_CLASS; iclass++) dataFile << results[iclass][ithread] << "\t";
        dataFile << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";

    return 0;
}


/*
 * Executes the following non-blocking (array based) queues in a single-enqueue-single-dequeue benchmark:
 * - LCRQ (lock-free)
 * - FAAArrayQueue (lock-free)
 * - MWC-LF (lock-free)
 * - MWC-WF (wait-free bounded)
 */
#include <iostream>
#include <fstream>
#include <cstring>

#include "datastructures/queues/LCRQueue.hpp"
#include "datastructures/queues/FAAArrayQueue.hpp"
#include "datastructures/queues/OFLFArrayLinkedListQueue.hpp"
#include "datastructures/queues/OFWFArrayLinkedListQueue.hpp"
#include "benchmarks/BenchmarkQueues.hpp"

#define MILLION  1000000LL

int main(void) {
    const std::string dataFilename {"data/q-array-enq-deq.txt"};
    vector<int> threadList = { 1, 2, 4, 8 };                 // For the laptop
    //vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64 }; // For Cervino
    const int numRuns = 1;                                   // Number of runs
    const long numPairs = 10*MILLION;                        // 10M is fast enough on the laptop, but on cervino we can use 100M
    const int EMAX_CLASS = 5;
    uint64_t results[EMAX_CLASS][threadList.size()];
    std::string cNames[EMAX_CLASS];
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*threadList.size());

    // Enq-Deq Throughput benchmarks
    for (int ithread = 0; ithread < threadList.size(); ithread++) {
        int nThreads = threadList[ithread];
        int iclass = 0;
        BenchmarkQueues bench(nThreads);
        std::cout << "\n----- q-array-enq-deq   threads=" << nThreads << "   pairs=" << numPairs/MILLION << "M   runs=" << numRuns << "-----\n";
        results[iclass++][ithread] = bench.enqDeq<FAAArrayQueue<UserData>>            (cNames[iclass], numPairs, numRuns);
        results[iclass++][ithread] = bench.enqDeq<LCRQueue<UserData>>                 (cNames[iclass], numPairs, numRuns);
        results[iclass++][ithread] = bench.enqDeq<OFLFArrayLinkedListQueue<UserData>> (cNames[iclass], numPairs, numRuns);
        results[iclass++][ithread] = bench.enqDeq<OFWFArrayLinkedListQueue<UserData>> (cNames[iclass], numPairs, numRuns);
        //results[iclass++][ithread] = bench.enqDeq<TinySTMLinkedListQueue<UserData>>(classNames[iclass], numPairs, numRuns);
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

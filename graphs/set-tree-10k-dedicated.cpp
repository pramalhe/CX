
/*
 * Executes the following linked list based sets
 * - Maged-Harris with Hazard Pointers (lock-free)
 * - Maged-Harris with Hazard Eras (lock-free)
 */
#include <iostream>
#include <fstream>
#include <cstring>

#include "common/UCSet.hpp"
#include "datastructures/sequential/TreeSet.hpp"
#include "ucs/FlatCombiningCRWWP.hpp"
#include "ucs/FlatCombiningLeftRight.hpp"
#include "ucs/PSim.hpp"
#include "ucs/PSimOpt.hpp"
#include "ucs/CXMutationWF.hpp"
#include "ucs/CXMutationWFTimed.hpp"
#include "benchmarks/BenchmarkSetsDedicated.hpp"


int main(void) {
    const std::string dataFilename {"data/set-tree-10k-dedicated.txt"};
    //vector<int> threadList = { 2, 4, 8 };                        // For the laptop
    vector<int> threadList = { 2, 4, 8, 16, 32, 48, 64 };      // For Cervino
    const int numElements = 1000000;                               // Number of keys in the set
    const int numRuns = 1;                                       // 5 runs for the paper
    const seconds testLength = 20s ;                              // 20s for the paper
    const int EMAX_CLASS = 10;
    TwoResults results[EMAX_CLASS][threadList.size()]; // One entry for read-only operations and another entry for update operations
    std::string cNames[EMAX_CLASS];
    int maxClass = 0;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*threadList.size());

    double totalHours = (double)EMAX_CLASS*threadList.size()*testLength.count()*numRuns/(60.*60.);
    std::cout << "This benchmark is going to take about " << totalHours << " hours to complete\n";

    for (unsigned ithread = 0; ithread < threadList.size(); ithread++) {
        auto nThreads = threadList[ithread];
        int iclass = 0;
        BenchmarkSetsDedicated bench(nThreads);
        std::cout << "\n----- Sets (Trees)   numElements=" << numElements << "   threads=" << nThreads << "   runs=" << numRuns << "   length=" << testLength.count() << "s -----\n";
        results[iclass++][ithread] = bench.benchmark<UCSet<FlatCombiningCRWWP<TreeSet<UserData>>,TreeSet<UserData>,UserData>,UserData>    (cNames[iclass], testLength, numRuns, numElements);
        results[iclass++][ithread] = bench.benchmark<UCSet<FlatCombiningLeftRight<TreeSet<UserData>>,TreeSet<UserData>,UserData>,UserData>(cNames[iclass], testLength, numRuns, numElements);
        results[iclass++][ithread] = bench.benchmark<UCSet<PSim<TreeSet<UserData>>,TreeSet<UserData>,UserData>,UserData>                  (cNames[iclass], testLength, numRuns, numElements);
        results[iclass++][ithread] = bench.benchmark<UCSet<PSimOpt<TreeSet<UserData>>,TreeSet<UserData>,UserData>,UserData>               (cNames[iclass], testLength, numRuns, numElements);
        results[iclass++][ithread] = bench.benchmark<UCSet<CXMutationWF<TreeSet<UserData>>,TreeSet<UserData>,UserData>,UserData>          (cNames[iclass], testLength, numRuns, numElements);
        results[iclass++][ithread] = bench.benchmark<UCSet<CXMutationWFTimed<TreeSet<UserData>>,TreeSet<UserData>,UserData>,UserData>          (cNames[iclass], testLength, numRuns, numElements);
        maxClass = iclass;
    }

    // Export tab-separated values to a file to be imported in gnuplot or excel
    ofstream dataFile;
    dataFile.open(dataFilename);
    dataFile << "Threads\t";
    // Printf class names
    for (int iclass = 0; iclass < maxClass; iclass++) dataFile << cNames[iclass] << "-Reads\t" <<  cNames[iclass] << "-Updates\t";
    dataFile << "\n";
    for (unsigned ithread = 0; ithread < threadList.size(); ithread++) {
        dataFile << threadList[ithread] << "\t";
        for (int iclass = 0; iclass < maxClass; iclass++) {
            dataFile << results[iclass][ithread].readops << "\t";
            dataFile << results[iclass][ithread].updateops << "\t";
        }
        dataFile << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";

    return 0;
}

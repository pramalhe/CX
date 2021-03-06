#include <iostream>
#include <fstream>
#include <cstring>

#include "common/UCSetBlocking.hpp"
#include "datastructures/lockfree/NatarajanTreeHE.hpp"
#include "datastructures/sequential/TreeSet.hpp"
#include "ucs/PSim.hpp"
#include "ucs/PSimOpt.hpp"
#include "ucs/CXMutationWF.hpp"
#include "ucs/CXMutationBlocking.hpp"
#include "ucs/CXMutationWFTimed.hpp"
#include "benchmarks/BenchmarkSets.hpp"


int main(void) {
    const std::string dataFilename {"data/set-treeblocking-1m.txt"};
    //vector<int> threadList = { 1, 2, 4, 8 };                     // For the laptop
    vector<int> threadList = { 1, 2, 4, 8, 16, 32}; // For Cervino
    vector<int> ratioList = { 1000};        // Permil ratio: 100%, 50%, 10%, 1%, 0.1%, 0%
    const int numElements = 1000000;                             // Number of keys in the set
    const int numRuns = 1;                                       // 5 runs for the paper
    const seconds testLength = 100s;                              // 20s for the paper
    const int EMAX_CLASS = 10;
    uint64_t results[EMAX_CLASS][threadList.size()][ratioList.size()];
    std::string cNames[EMAX_CLASS];
    int maxClass = 0;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*threadList.size()*ratioList.size());

    double totalHours = (double)EMAX_CLASS*ratioList.size()*threadList.size()*testLength.count()*numRuns/(60.*60.);
    std::cout << "This benchmark is going to take about " << totalHours << " hours to complete\n";

    for (unsigned iratio = 0; iratio < ratioList.size(); iratio++) {
        auto ratio = ratioList[iratio];
        for (unsigned ithread = 0; ithread < threadList.size(); ithread++) {
            auto nThreads = threadList[ithread];
            int iclass = 0;
            BenchmarkSets bench(nThreads);
            std::cout << "\n----- Sets (Trees)   numElements=" << numElements << "   ratio=" << ratio/10. << "%   threads=" << nThreads << "   runs=" << numRuns << "   length=" << testLength.count() << "s -----\n";
            results[iclass++][ithread][iratio] = bench.benchmark<UCSetBlocking<CXMutationBlocking<TreeSet<UserData>>,TreeSet<UserData>,UserData>,UserData>          (cNames[iclass], ratio, testLength, numRuns, numElements, false, 2);
            results[iclass++][ithread][iratio] = bench.benchmark<UCSetBlocking<CXMutationBlocking<TreeSet<UserData>>,TreeSet<UserData>,UserData>,UserData>          (cNames[iclass], ratio, testLength, numRuns, numElements, false, 4);
            results[iclass++][ithread][iratio] = bench.benchmark<UCSetBlocking<CXMutationBlocking<TreeSet<UserData>>,TreeSet<UserData>,UserData>,UserData>          (cNames[iclass], ratio, testLength, numRuns, numElements, false, 6);
            results[iclass++][ithread][iratio] = bench.benchmark<UCSetBlocking<CXMutationBlocking<TreeSet<UserData>>,TreeSet<UserData>,UserData>,UserData>          (cNames[iclass], ratio, testLength, numRuns, numElements, false, 8);
            results[iclass++][ithread][iratio] = bench.benchmark<UCSetBlocking<CXMutationBlocking<TreeSet<UserData>>,TreeSet<UserData>,UserData>,UserData>          (cNames[iclass], ratio, testLength, numRuns, numElements, false, 16);
            results[iclass++][ithread][iratio] = bench.benchmark<UCSetBlocking<CXMutationBlocking<TreeSet<UserData>>,TreeSet<UserData>,UserData>,UserData>          (cNames[iclass], ratio, testLength, numRuns, numElements, false, 32);                                     (cNames[iclass], ratio, testLength, numRuns, numElements, false);
            maxClass = iclass;
        }
    }

    // Export tab-separated values to a file to be imported in gnuplot or excel
    ofstream dataFile;
    dataFile.open(dataFilename);
    dataFile << "Threads\t";
    // Printf class names and ratios for each column
    for (unsigned iratio = 0; iratio < ratioList.size(); iratio++) {
        auto ratio = ratioList[iratio];
        for (int iclass = 0; iclass < maxClass; iclass++) dataFile << cNames[iclass] << "-" << ratio/10. << "%"<< "\t";
    }
    dataFile << "\n";
    for (int ithread = 0; ithread < threadList.size(); ithread++) {
        dataFile << threadList[ithread] << "\t";
        for (unsigned iratio = 0; iratio < ratioList.size(); iratio++) {
            for (int iclass = 0; iclass < maxClass; iclass++) dataFile << results[iclass][ithread][iratio] << "\t";
        }
        dataFile << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";

    return 0;
}

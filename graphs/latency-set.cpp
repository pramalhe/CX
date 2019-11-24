#include <iostream>
#include <fstream>
#include <cstring>

#include "../common/UCSet.hpp"                  // TODO: replace with UCSet, when we have everything using "T key" instead of "T* key"
#include "datastructures/lockfree/MagedHarrisLinkedListSetHP.hpp"
#include "datastructures/lockfree/MagedHarrisLinkedListSetHE.hpp"
#include "datastructures/sequential/LinkedListSet.hpp"
#include "ucs/PSimOpt.hpp"
#include "ucs/PSim.hpp"
#include "ucs/CXMutationWF.hpp"
#include "benchmarks/BenchmarkLatencySets.hpp"


int main(void) {
    const std::string dataFilename {"data/latency-set.txt"};
    vector<int> threadList = { 1, 2, 4, 8 };
    //vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64 }; // For Cervino
    const int numElements = 1000;                                  // Number of keys in the set
    const int EMAX_CLASS = 10;
    uint64_t results[EMAX_CLASS][threadList.size()];
    std::string cNames[EMAX_CLASS];
    int maxClass = 0;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*threadList.size());

    for (unsigned ithread = 0; ithread < threadList.size(); ithread++) {
        auto nThreads = threadList[ithread];
        BenchmarkLatencySets<UserData> bench(nThreads);
        std::cout << "\n----- Latency for Sets (Linked-Lists)   numElements=" << numElements << "   threads=" << nThreads << " -----\n";
        results[1][ithread] = bench.latency<UCSet<CXMutationWF<LinkedListSet<UserData>>,LinkedListSet<UserData>,UserData>>          (cNames[1], numElements);
    }
    for (unsigned ithread = 0; ithread < threadList.size(); ithread++) {
        auto nThreads = threadList[ithread];
        BenchmarkLatencySets<UserData> bench(nThreads);
        std::cout << "\n----- Latency for Sets (Linked-Lists)   numElements=" << numElements << "   threads=" << nThreads << " -----\n";
        results[2][ithread] = bench.latency<MagedHarrisLinkedListSetHP<UserData>>                                                   (cNames[2], numElements);
    }

    // Export tab-separated values to a file to be imported in gnuplot or excel
    ofstream dataFile;
    dataFile.open(dataFilename);
    dataFile << "Threads\t";
    // Printf class names for each column
    for (int iclass = 0; iclass < maxClass; iclass++) dataFile << cNames[iclass] << "\t";
    dataFile << "\n";
    for (int ithread = 0; ithread < threadList.size(); ithread++) {
        dataFile << threadList[ithread] << "\t";
        for (int iclass = 0; iclass < maxClass; iclass++) dataFile << results[iclass][ithread] << "\t";
        dataFile << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";

    return 0;
}

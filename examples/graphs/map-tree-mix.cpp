
/*
 * Executes the following linked list based sets
 * - Natarajan Tree with Hazard Eras (lock-free)
 * - MWC-LF Red-Black Tree (lock-free)
 * - MWC-WF Red-Black Tree (wait-free bounded)
 * - TinySTM (blocking)
 * - TODO AVL from tinystm (to put in MWC) ?
 * - TODO flat tree (array to put in MWC) ?
 */
//#include "datastructures/treemaps/NatarajanTreeHE.hpp"
#include "../common/UCSet.hpp"                  // TODO: replace with UCSet, when we have everything using "T key" instead of "T* key"
#include "datastructures/lockfree/MagedHarrisLinkedListSetHP.hpp"
#include "datastructures/lockfree/MagedHarrisLinkedListSetHE.hpp"
#include "datastructures/sequential/TreeSet.hpp"
#include "ucs/FlatCombiningCRWWP.hpp"
#include "ucs/FlatCombiningLeftRight.hpp"
#include "ucs/PSimOpt.hpp"
#include "ucs/CXMutationWF.hpp"
#include "ucs/CXMutationWFTimed.hpp"
#include "benchmarks/BenchmarkMaps.hpp"


int main(void) {
    vector<int> threadList = { 1, 2, 4, 8 };                 // For the laptop
    //vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64, 72, 80, 88, 96 }; // For Cervino
    vector<int> ratioList = { 1000, 500, 100, 10, 1, 0 };    // Permil ratio: 100%, 50%, 10%, 1%, 0.1%, 0%
    vector<long long> elemsList = { 1000 };                  // Number of keys in the set
    const int numRuns = 1;                                   // 5 runs for the paper
    const seconds testLength = 2s;                           // 20s for the paper
    const int EMAX_STRUCT = 4;

    long long ops[EMAX_STRUCT][elemsList.size()][ratioList.size()][threadList.size()];

    double totalHours = (double)EMAX_STRUCT*elemsList.size()*ratioList.size()*threadList.size()*testLength.count()*numRuns/(60.*60.);
    std::cout << "This benchmark is going to take about " << totalHours << " hours to complete\n";

    for (unsigned ielem = 0; ielem < elemsList.size(); ielem++) {
        auto numElements = elemsList[ielem];
        for (unsigned iratio = 0; iratio < ratioList.size(); iratio++) {
            auto ratio = ratioList[iratio];
            for (unsigned ithread = 0; ithread < threadList.size(); ithread++) {
                // Initialize operation counters
                for (int itype = 0; itype < EMAX_STRUCT; itype++) ops[itype][ielem][iratio][ithread] = 0;
                auto nThreads = threadList[ithread];
                BenchmarkMaps bench(nThreads);
                int iclass = 0;
                std::cout << "\n----- Sets Benchmark   numElements=" << numElements << "   ratio=" << ratio/10. << "%   threads=" << nThreads << "   runs=" << numRuns << "   length=" << testLength.count() << "s -----\n";
                //ops[iclass++][ielem][iratio][ithread] = bench.benchmark<NatarajanTreeHE,uint64_t,uint64_t>(ratio, testLength, numRuns, numElements, false);
                //ops[iclass++][ielem][iratio][ithread] = bench.benchmark<MagedHarrisLinkedListSetHP<UserData>,UserData>(ratio, testLength, numRuns, numElements, false);
            }
        }
    }

    // Show results in tab format to import on gnuplot
    for (unsigned ielem = 0; ielem < elemsList.size(); ielem++) {
        auto numElements = elemsList[ielem];
        for (unsigned iratio = 0; iratio < ratioList.size(); iratio++) {
            auto ratio = ratioList[iratio]/10.;
            std::cout << "Ratio " << ratio << "%\n";
            std::cout << "Threads\n";
            for (unsigned ithread = 0; ithread < threadList.size(); ithread++) {
                auto nThreads = threadList[ithread];
                std::cout << nThreads << ", ";
                for (int il = 0; il < EMAX_STRUCT; il++) {
                    std::cout << ops[il][ielem][iratio][ithread] << "\t";
                }
                std::cout << "\n";
            }
        }
    }

    return 0;
}

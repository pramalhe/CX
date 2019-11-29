CXX = g++-8
CXXFLAGS = -std=c++17 -g -O3 -I../

UCDEPS = \
	../ucs/CXMutationWF.hpp \
	../ucs/CXMutationWFTimed.hpp \
	../ucs/HerlihyUniversal.hpp \
	../ucs/PSimOpt.hpp \
	../common/CircularArray.hpp \
	../common/HazardEras.hpp \
	../common/HazardPointers.hpp \
	../common/HazardPointersSimQueue.hpp \
	../common/StrongTryRIRWLock.hpp \
	../common/UCSet.hpp \
	../common/UCQueue.hpp \
	../common/URCUReadersVersion.hpp \
	../datastructures/lockfree/COWSortedVectorSet.hpp \
	../datastructures/lockfree/MagedHarrisLinkedListSetHE.hpp \
	../datastructures/lockfree/MagedHarrisLinkedListSetHP.hpp \
	../datastructures/lockfree/NatarajanTreeHE.hpp \
	../datastructures/sequential/SortedVectorSet.hpp \
	../datastructures/sequential/SortedArraySet.hpp \
	../datastructures/sequential/TreeSet.hpp \
	../datastructures/waitfree/WFRBT.hpp \

BINARIES = \
	bin/q-ll-enq-deq \
	bin/set-ll-1k \
	bin/set-ll-10k \
	bin/set-tree-1k \
	bin/set-tree-10k \
	bin/set-tree-1m \
	bin/set-hash-1k \
	bin/set-hash-1m \
	bin/latency-set \
	bin/set-tree-10k-dedicated \
	bin/set-treeblocking-1m \
	bin/set-treeblocking-10m \
#	bin/q-array-enq-deq \
#	bin/q-ll-burst \
#	bin/set-ll-mix \
#	bin/set-tree-mix \
	bin/set-treeblocking-1m \

all: $(BINARIES)

run:
	bin/q-ll-enq-deq
	bin/set-ll-1k
	bin/set-ll-10k
	bin/set-tree-1k
	bin/set-tree-10k
	bin/set-tree-1m
	bin/set-hash-1k
	bin/set-hash-1m
	bin/latency-set
	bin/set-tree-10k-dedicated

clean:
	rm -f bin/*

#
# Queues
#	
bin/q-ll-enq-deq: q-ll-enq-deq.cpp $(UCDEPS)
	$(CXX) $(CXXFLAGS) q-ll-enq-deq.cpp -o bin/q-ll-enq-deq -lpthread $(LIBS)
	
bin/q-array-enq-deq: q-array-enq-deq.cpp $(UCDEPS)
	$(CXX) $(CXXFLAGS) q-array-enq-deq.cpp -o bin/q-array-enq-deq -lpthread	$(LIBS)
	
bin/q-ll-burst: q-ll-burst.cpp
	$(CXX) $(CXXFLAGS) q-ll-burst.cpp -o bin/q-ll-burst -lpthread $(LIBS)

	
#
# Sets
#	
bin/set-ll-1k: set-ll-1k.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) $(CXXFLAGS) set-ll-1k.cpp -o bin/set-ll-1k -lpthread $(LIBS)

bin/set-ll-10k: set-ll-10k.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) $(CXXFLAGS) set-ll-10k.cpp -o bin/set-ll-10k -lpthread $(LIBS)

bin/set-tree-1k: set-tree-1k.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) $(CXXFLAGS) set-tree-1k.cpp -o bin/set-tree-1k -lpthread $(LIBS)

bin/set-tree-10k: set-tree-10k.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) $(CXXFLAGS) set-tree-10k.cpp -o bin/set-tree-10k -lpthread $(LIBS)

bin/set-tree-1m: set-tree-1m.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) $(CXXFLAGS) set-tree-1m.cpp -o bin/set-tree-1m -lpthread $(LIBS)
	
bin/set-treeblocking-1m: set-treeblocking-1m.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) $(CXXFLAGS) -DTREEBLOCKING set-treeblocking-1m.cpp -o bin/set-treeblocking-1m -lpthread $(LIBS)
	
bin/set-treeblocking-10m: set-treeblocking-10m.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) $(CXXFLAGS) -DTREEBLOCKING set-treeblocking-10m.cpp -o bin/set-treeblocking-10m -lpthread $(LIBS)
		
bin/set-hash-1k: set-hash-1k.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) $(CXXFLAGS) set-hash-1k.cpp -o bin/set-hash-1k -lpthread $(LIBS)

bin/set-hash-1m: set-hash-1m.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) $(CXXFLAGS) set-hash-1m.cpp -o bin/set-hash-1m -lpthread $(LIBS)

#
# Latency
#
bin/latency-set: latency-set.cpp $(UCDEPS) ../benchmarks/BenchmarkLatencySets.hpp
	$(CXX) $(CXXFLAGS) latency-set.cpp -o bin/latency-set -lpthread $(LIBS)

	
	

#
# Dedicated
#	
bin/set-tree-10k-dedicated: set-tree-10k-dedicated.cpp $(UCDEPS) ../benchmarks/BenchmarkSetsDedicated.hpp
	$(CXX) $(CXXFLAGS) set-tree-10k-dedicated.cpp -o bin/set-tree-10k-dedicated -lpthread $(LIBS)




CXX = g++-8
CXXFLAGS = 
INCLUDES = -I../

UCDEPS = \
	../ucs/CRWWPUniversal.hpp \
	../ucs/CXMutationWF.hpp \
	../ucs/CXMutationWFTimed.hpp \
	../ucs/FlatCombiningCRWWP.hpp \
	../ucs/FlatCombiningLeftRight.hpp \
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
#	bin/q-array-enq-deq \
#	bin/q-ll-burst \
#	bin/set-ll-mix \
#	bin/set-tree-mix \
	bin/set-treeblocking-1m \

all: $(BINARIES)


clean:
	rm -f bin/*

#
# Queues
#	
bin/q-ll-enq-deq: q-ll-enq-deq.cpp $(UCDEPS)
	$(CXX) -std=c++17 -fuse-ld=gold -g -O3 $(INCLUDES) q-ll-enq-deq.cpp -o bin/q-ll-enq-deq -lpthread $(LIBS)
	
bin/q-array-enq-deq: q-array-enq-deq.cpp $(UCDEPS)
	$(CXX) -std=c++17 -fuse-ld=gold -g -O3 $(INCLUDES) q-array-enq-deq.cpp -o bin/q-array-enq-deq -lpthread	$(LIBS)
	
bin/q-ll-burst: q-ll-burst.cpp
	$(CXX) -std=c++17 -fuse-ld=gold -g -O3 $(INCLUDES) q-ll-burst.cpp -o bin/q-ll-burst -lpthread $(LIBS)

	
#
# Sets
#	
bin/set-ll-1k: set-ll-1k.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) -std=c++17 -fuse-ld=gold -g -O3 $(INCLUDES) set-ll-1k.cpp -o bin/set-ll-1k -lpthread $(LIBS)

bin/set-ll-10k: set-ll-10k.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) -std=c++17 -fuse-ld=gold -g -O3 $(INCLUDES) set-ll-10k.cpp -o bin/set-ll-10k -lpthread $(LIBS)

bin/set-tree-1k: set-tree-1k.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) -std=c++17 -fuse-ld=gold -g -O3 $(INCLUDES) set-tree-1k.cpp -o bin/set-tree-1k -lpthread $(LIBS)

bin/set-tree-10k: set-tree-10k.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) -std=c++17 -fuse-ld=gold -g -O3 $(INCLUDES) set-tree-10k.cpp -o bin/set-tree-10k -lpthread $(LIBS)

bin/set-tree-1m: set-tree-1m.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) -std=c++17 -fuse-ld=gold -g -O3 $(INCLUDES) set-tree-1m.cpp -o bin/set-tree-1m -lpthread $(LIBS)
	
bin/set-treeblocking-1m: set-treeblocking-1m.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) -std=c++17 -fuse-ld=gold -g -O3 $(INCLUDES) set-treeblocking-1m.cpp -o bin/set-treeblocking-1m -lpthread $(LIBS)
	
bin/set-hash-1k: set-hash-1k.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) -std=c++17 -fuse-ld=gold -g -O3 $(INCLUDES) set-hash-1k.cpp -o bin/set-hash-1k -lpthread $(LIBS)

bin/set-hash-1m: set-hash-1m.cpp $(UCDEPS) ../benchmarks/BenchmarkSets.hpp
	$(CXX) -std=c++17 -fuse-ld=gold -g -O3 $(INCLUDES) set-hash-1m.cpp -o bin/set-hash-1m -lpthread $(LIBS)

#
# Latency
#
bin/latency-set: latency-set.cpp $(UCDEPS) ../benchmarks/BenchmarkLatencySets.hpp
	$(CXX) -std=c++17 -fuse-ld=gold -g -O3 $(INCLUDES) latency-set.cpp -o bin/latency-set -lpthread $(LIBS)

	
	

#
# Maps.. TODO
#
bin/set-tree-mix: set-tree-mix.cpp $(UCDEPS) ../benchmarks/BenchmarkMaps.hpp
	$(CXX) -std=c++14 -fuse-ld=gold -g -O3 $(INCLUDES) set-tree-mix.cpp -o bin/set-tree-mix -lpthread $(LIBS)
	
#
# Dedicated
#	
bin/set-tree-10k-dedicated: set-tree-10k-dedicated.cpp $(UCDEPS) ../benchmarks/BenchmarkSetsDedicated.hpp
	$(CXX) -std=c++17 -fuse-ld=gold -g -O3 $(INCLUDES) set-tree-10k-dedicated.cpp -o bin/set-tree-10k-dedicated -lpthread $(LIBS)




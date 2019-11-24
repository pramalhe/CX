# CX

A Wait-Free Universal Construct for Large Objects

Make any data structure wait-free as long as it has a copy constructor. No annotation needed.

## Build and run the benchmarks ##
To build the benchmarks go into the graphs folder and type make

	cd graphs/
	make
	
This will generate the following benchmarks in the bin/ folder:

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

Building CX requires a compiler with C++14 support. If you want to build Natarajan's tree then you'll need C++17 support due to the usage of std::pair.
	
## Using CX in your code ##
To use CX and one of the data structures in your own code, you'll need the following files:

	ucs/CXMutationWF.hpp
	common/CircularArray.hpp
	common/StrongTryRIRWLock.hpp
	common/HazardPointersCX.hpp
	
There is a small example in:
 
	examples/example1.cpp



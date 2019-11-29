# CX

A Wait-Free Universal Construct for Large Objects

Make any data structure wait-free as long as it has a copy constructor. No annotation needed.

## Build and run the benchmarks ##
To build the benchmarks go into the graphs folder and type make

	cd graphs/
	make
	
This will generate the following benchmarks executables in the bin/ folder:

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
	bin/set-treeblocking-1m
	bin/set-treeblocking-10m

Building CX requires a compiler with C++14 support. If you want to build Natarajan's tree then you'll need C++17 support due to the usage of std::pair.

Still in the graphs/ folder type 'make run'. This will run the relevant benchmarks, saving the results of each in data/filename.txt

	make run


## Using CX in your code ##
To use CX and one of the data structures in your own code, you'll need the following files:

	ucs/CXMutationWF.hpp
	common/CircularArray.hpp
	common/StrongTryRIRWLock.hpp
	common/HazardPointersCX.hpp
	
There is a small example in:
 
	examples/example1.cpp

If you want to see the actual of the universal construction, take a look at:

    ucs/CXMutationWF.hpp

An older version of a technical explaning how it works can be found in the main folder:

    CX-TechReport-2018.pdf

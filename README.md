# CX

A Wait-Free Universal Construct for Large Objects

Make any data structure wait-free as long as it has a copy constructor. No annotation needed.

## Build and run the benchmarks ##
To build the benchmarks go into the graphs folder and type make

	cd graphs/
	make
	
This will generate the following benchmarks in the bin folder
	
	
	
## Using CX in your code ##
To use CX and one of the data structures in your own code, you'll need the following files:

	ucs/CXMutationWF.hpp
	common/CircularArray.hpp
	common/StrongTryRIRWLock.hpp
	common/HazardPointersCX.hpp
	
There is a small example in:
 
	examples/example1.cpp
build: cfg graph builder
	g++ -g -O0 -o cs-analyzer CFG.o BasicBlock.o CFGraph.o CFGBuilder.o
	
graph: BasicBlock.h BasicBlock.cpp CFGraph.h CFGraph.cpp
	g++ -g -O0 -c BasicBlock.cpp
	g++ -g -O0 -c CFGraph.cpp

builder: CFGBuilder.h CFGBuilder.cpp
	g++ -g -O0 -c CFGBuilder.cpp
	
cfg: CFG.cpp
	g++ -g -O0 -c CFG.cpp
	
clean:
	rm -f *.o *.bak cs-analyzer
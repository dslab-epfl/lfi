build: graph builder mainx manager
	g++ -g -O0 -o profiler BasicBlock.o CFGraph.o CFGBuilder.o CFG.o
	
graph: BasicBlock.h BasicBlock.cpp CFGraph.h CFGraph.cpp
	g++ -g -O0 -c BasicBlock.cpp
	g++ -g -O0 -c CFGraph.cpp

builder: CFGBuilder.h CFGBuilder.cpp
	g++ -g -O0 -c CFGBuilder.cpp
	
mainx: CFG.cpp
	g++ -g -O0 -c CFG.cpp
	
	
manager: ProfilerMgr.cpp
	g++ -g -O0 -o profilermgr ProfilerMgr.cpp
	
clean:
	rm -f *.o *.bak profiler profilermgr
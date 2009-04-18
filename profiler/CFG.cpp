/*
     Created by Paul Marinescu and George Candea
     Copyright (C) 2009 EPFL (Ecole Polytechnique Federale de Lausanne)

     This file is part of LFI (Library-level Fault Injector).

     LFI is free software: you can redistribute it and/or modify it  
     under the terms of the GNU General Public License as published by the  
     Free Software Foundation, either version 3 of the License, or (at  
     your option) any later version.

     LFI is distributed in the hope that it will be useful, but  
     WITHOUT ANY WARRANTY; without even the implied warranty of  
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  
     General Public License for more details.

     You should have received a copy of the GNU General Public  
     License along with LFI. If not, see http://www.gnu.org/licenses/.

     EPFL
     Dependable Systems Lab (DSLAB)
     Room 330, Station 14
     1015 Lausanne
     Switzerland
*/

#include <fstream>
#include <iostream>
#include <string>
#include <queue>
#include <set>

#include "CFGBuilder.h"
#include "CFGBuilderSPARC.h"

#define MAGIC_FNPTR_CALL	"call 00000000 <function_pointer_call>"

using namespace std;

struct BFPathElement
{
	int bbId;
	string target;
	
	int bbIdParent;
	string targetParent;
};

bool operator<(const BFPathElement& lhs, const BFPathElement& rhs)
{
	if (lhs.bbId < rhs.bbId)
		return true;
	if (lhs.bbId == rhs.bbId && lhs.target < rhs.target)
		return true;
	
	return false;
}

struct SideEffectElement
{
	int bbId;
	int type; /* 0 - global variable, 1 - argument */
	set<string> targets;
};


int CompareSEElements(const SideEffectElement& e1, const SideEffectElement& e2)
{
	set<string>::const_iterator it1, it2;

	for (it1 = e1.targets.begin(),it2 = e2.targets.begin();
		it1 != e1.targets.end() && it2 != e2.targets.end();
		++it1, ++it2)
	{
		if (*it1 < *it2)
			return -1;
		if (*it1 > *it2)
			return 1;
	}

	return 0;
}

bool operator<(const SideEffectElement& lhs, const SideEffectElement& rhs)
{
	if (lhs.bbId < rhs.bbId)
		return true;
	if (lhs.bbId == rhs.bbId && lhs.targets.size() < rhs.targets.size())
		return true;
	
	if (lhs.bbId == rhs.bbId && lhs.targets.size() == rhs.targets.size())
		return (-1 == CompareSEElements(lhs, rhs));
	
	return false;
}
void prel(ostream& outf, const SideEffectElement& e);

void prel(const SideEffectElement& e)
{
	return prel(cerr, e);
}

void prel(ostream& outf, const SideEffectElement& e)
{
	outf << "id " << e.bbId << endl;
	outf << "size " << e.targets.size() << endl;
	for (set<string>::const_iterator it = e.targets.begin(); 
		it != e.targets.end();
		++it)
	{
		outf << *it << " " << endl;
	}
}

bool IsIncluded(SideEffectElement& s, const SideEffectElement& e)
{
	set<string>::const_iterator it1;
	bool rv = true;

	it1 = e.targets.begin();
	
	for (; it1 != e.targets.end(); ++it1)
	{
		if (s.targets.insert(*it1).second)
			rv = false;
	}

	return rv;
}

typedef vector<BFPathElement> BFPath;

string GetText(char* pszAsmPath)
{
	string line, contents;
	ifstream inf(pszAsmPath);

	if (inf.is_open())
	{
		while (!inf.eof() && inf.good())
		{
			getline(inf, line);
			contents += line;
			contents += "\n";
		}
	}
	return contents;
}

int IsRegisterx86(string target)
{
	return (target[0] != 'D' && target[0] != '[');
}

int IsRegisterSPARC(string target)
{
	return (target[0] == '%');
}

void ReconstructPath(const BFPathElement& element, const set<BFPathElement>& visited, set<int>& targetBBs)
{
	BFPathElement el;
	set<BFPathElement>::iterator it;

	targetBBs.insert(element.bbId);
	el.bbId = element.bbIdParent;
	el.target = element.targetParent;

	if ((it = visited.find(el)) != visited.end())
	{
		ReconstructPath(*it, visited, targetBBs);
	}
}

/*
 * Preliminary side-effect via arguments detection
 * The algorithm uses the same algorithm as the return value detection but
 * - searches forward into the CFG
 * - starts with [ebp + ??] as the target
*/
int BFSideEffectsx862(CFGraph* graph, int start, const set<int>& targetBBs)
{
	CFGraph::AdjListElement* head;
	queue<SideEffectElement> bfQueue;

	SideEffectElement anElement, originalElement;

	BasicBlock *aBB;
	list<string>::const_iterator itr;
	SideEffectElement setVisited[1000];

	string op1, op2, target, instruction;

	set<int> bbOfInterest;
	int i, j, justcalled;

	if (head = graph->GetHead(start))
	{
		/* watching max 7 arguments. increase if needed */
		anElement.targets.insert("[ebp+0x8]");
		anElement.targets.insert("[ebp+0xc]");
		anElement.targets.insert("[ebp+0x10]");
		anElement.targets.insert("[ebp+0x14]");
		anElement.targets.insert("[ebp+0x18]");
		anElement.targets.insert("[ebp+0x1c]");
		anElement.targets.insert("[ebp+0x20]");

		do {
			anElement.bbId = head->m_bbIndex;
			bfQueue.push(anElement);
		} while (head = head->m_pNext);
	}

	while (!bfQueue.empty())
	{
		anElement = bfQueue.front();
		bfQueue.pop();

		originalElement = anElement;
		aBB = graph->m_vBasicBlocks[anElement.bbId];
		if (!aBB || anElement.bbId == EXIT_NODE)
			continue;

		for (itr = aBB->m_listInstructions.begin();
			itr != aBB->m_listInstructions.end();
			++itr)
		{
			i = 0;
			while (' ' != (*itr)[++i]);
			instruction = itr->substr(0, i);


			if ("mov" == instruction || "movzx" == instruction)
			{
				/* we search for either mov reg, lvl1Target -> reg = lvl2Target or
				 *  mov [lvl2Target+?], ? where ? can be anything
				*/
				
				i = 3;
				while(' ' == (*itr)[++i]);
				if ((*itr)[i] == 'D') //DWORD PTR
					i += 10;
				else if ((*itr)[i] == 'B') //BYTE PTR
					i += 9;

				j = i;
				while(',' != (*itr)[++j]);
				op1 = itr->substr(i, j-i);
				if ((*itr)[j+1] == 'D') //DWORD PTR
					j += 10;
				op2 = itr->substr(j+1);

				if (anElement.targets.find(op2) != anElement.targets.end())
				{
					anElement.targets.insert(op1);
				}
				else if (op1[0] == '[' && op1[1] == 'e')
				{
					target = op1.substr(1, 3);
					if (anElement.targets.find(target) != anElement.targets.end())
					{
						bbOfInterest.insert(anElement.bbId);
					}
				}
				else if (anElement.targets.find(op1) != anElement.targets.end())
				{
					anElement.targets.erase(anElement.targets.find(op1));
				}
			}
			else if ("xor" == instruction || "lea" == instruction)
			{
				i = 3;
				while(' ' == (*itr)[++i]);
				j = i;
				while(',' != (*itr)[++j]);
				op1 = itr->substr(i, j-i);
				op2 = itr->substr(j+1);
				
				if (anElement.targets.find(op1) != anElement.targets.end())
				{
					anElement.targets.erase(anElement.targets.find(op1));
				}
			}
			else if ("or" == instruction)
			{
				i = 2;
				while(' ' == (*itr)[++i]);
				j = i;
				while(',' != (*itr)[++j]);
				op1 = itr->substr(i, j-i);
				op2 = itr->substr(j+1);
				
				if (anElement.targets.find(op1) != anElement.targets.end())
				{
					anElement.targets.erase(anElement.targets.find(op1));
				}
			}
		}

		if (head = graph->GetHead(anElement.bbId))
		{
			do {
				anElement.bbId = head->m_bbIndex;

				if (!IsIncluded(setVisited[anElement.bbId], anElement))
				{
					anElement.targets = setVisited[anElement.bbId].targets;
					bfQueue.push(anElement);
				}
			} while (head = head->m_pNext);
		}
	}

	for (set<int>::iterator it = bbOfInterest.begin();
		it != bbOfInterest.end();
		++it)
	{
		cerr << *it << endl;
		if (targetBBs.find(*it) != targetBBs.end())
			return 1;
	}
	return 0;
}

/*
 * Preliminary side-effect via global variables detection
 * The algorithm uses the same algorithm as the return value detection but
 * - searches forward into the CFG
 * - starts with registers that contain the base address used for global
     variable access (usually ebx or ecx are used by x86 PIC) as the target
*/

int BFSideEffectsx86(CFGraph* graph, int start, const set<int>& targetBBs)
{
	CFGraph::AdjListElement* head;
	queue<SideEffectElement> bfQueue;

	SideEffectElement anElement, originalElement;

	BasicBlock *aBB;
	list<string>::const_iterator itr;
	SideEffectElement setVisited[1000];

	string op1, op2, target, instruction;

	set<int> bbOfInterest;
	int i, j, justcalled;

	if (graph->m_vBasicBlocks.size() > 1)
	{
		aBB = graph->m_vBasicBlocks[1];
		justcalled = 0;
		for (itr = aBB->m_listInstructions.begin();
			itr != aBB->m_listInstructions.end();
			++itr)
		{
			i = 0;
			while (' ' != (*itr)[++i]);
			instruction = itr->substr(0, i);
			if ("call" == instruction)
			{
				justcalled = 1;
			}
			else if (justcalled && ("add" == instruction || "sub" == instruction))
			{
				i = 3;
				while(' ' == (*itr)[++i]);

				j = i;
				while(',' != (*itr)[++j]);
				op1 = itr->substr(i, j-i);
				op2 = itr->substr(j+1);
				
				if ('e' == op1[0] && '0' == op2[0])
				{
					anElement.targets.insert(op1);
				}
			}
			else
			{
				justcalled = 0;
			}
		}
	}

	while (!bfQueue.empty())
	{
		anElement = bfQueue.front();
		bfQueue.pop();

		originalElement = anElement;
		aBB = graph->m_vBasicBlocks[anElement.bbId];
		if (!aBB || anElement.bbId == EXIT_NODE)
			continue;

		for (itr = aBB->m_listInstructions.begin();
			itr != aBB->m_listInstructions.end();
			++itr)
		{
			i = 0;
			while (' ' != (*itr)[++i]);
			instruction = itr->substr(0, i);


			if ("mov" == instruction || "movzx" == instruction)
			{
				/* we search for either mov reg, lvl1Target -> reg = lvl2Target or
				 *  mov [lvl2Target+?], ? where ? can be anything
				*/
				
				i = 3;
				while(' ' == (*itr)[++i]);
				if ((*itr)[i] == 'D') //DWORD PTR
					i += 10;
				else if ((*itr)[i] == 'B') //BYTE PTR
					i += 9;

				j = i;
				while(',' != (*itr)[++j]);
				op1 = itr->substr(i, j-i);
				if ((*itr)[j+1] == 'D') //DWORD PTR
					j += 10;
				op2 = itr->substr(j+1);

				if (anElement.targets.find(op2) != anElement.targets.end())
				{
					anElement.targets.insert(op1);
				}
				else if (op1[0] == '[' && op1[1] == 'e')
				{
					target = op1.substr(1, 3);
					if (anElement.targets.find(target) != anElement.targets.end())
					{
						bbOfInterest.insert(anElement.bbId);
					}
				}
				else if (anElement.targets.find(op1) != anElement.targets.end())
				{
					anElement.targets.erase(anElement.targets.find(op1));
				}
			}
			else if ("xor" == instruction || "lea" == instruction)
			{
				i = 3;
				while(' ' == (*itr)[++i]);
				j = i;
				while(',' != (*itr)[++j]);
				op1 = itr->substr(i, j-i);
				op2 = itr->substr(j+1);
				
				if (anElement.targets.find(op1) != anElement.targets.end())
				{
					anElement.targets.erase(anElement.targets.find(op1));
				}
			}
			else if ("or" == instruction)
			{
				i = 2;
				while(' ' == (*itr)[++i]);
				j = i;
				while(',' != (*itr)[++j]);
				op1 = itr->substr(i, j-i);
				op2 = itr->substr(j+1);
				
				if (anElement.targets.find(op1) != anElement.targets.end())
				{
					anElement.targets.erase(anElement.targets.find(op1));
				}
			}
		}

		if (head = graph->GetHead(anElement.bbId))
		{
			do {
				anElement.bbId = head->m_bbIndex;

				if (!IsIncluded(setVisited[anElement.bbId], anElement))
				{
					anElement.targets = setVisited[anElement.bbId].targets;
					bfQueue.push(anElement);
				}
			} while (head = head->m_pNext);
		}
	}

	for (set<int>::iterator it = bbOfInterest.begin();
		it != bbOfInterest.end();
		++it)
	{
		cerr << *it << endl;
		if (targetBBs.find(*it) != targetBBs.end())
			return 1;
	}
	return 0;
}


void BFWalkx86(CFGraph* graph, int start, set<int>& targetBBs, char* reffile)
{
	CFGraph::AdjListElement* head;
	queue<BFPathElement> bfQueue;
	
	BFPathElement anElement;

	BasicBlock* aBB;
	list<string>::const_reverse_iterator itr;
	set<BFPathElement> setVisited;

	ofstream outref;
	int directCalls = 0, indirectCalls = 0;
	
	string target, aTarget, instruction;

	int i, j;
	int retcall;

	if (reffile)
	{
		outref.open(reffile);
		if (!outref.is_open())
		{
			cerr << "Unable to open " << reffile << endl;
			reffile = NULL;
		}
	}

	if (head = graph->GetHead(start))
	{
		anElement.target = "eax";
		do {
			anElement.bbId = head->m_bbIndex;
			bfQueue.push(anElement);
			setVisited.insert(anElement);
		} while (head = head->m_pNext);
	}
	while (!bfQueue.empty())
	{
		anElement = bfQueue.front();
		bfQueue.pop();

		target = anElement.target;

		aBB = graph->m_vBasicBlocks[anElement.bbId];

		retcall = 0;
		for (itr = aBB->m_listInstructions.rbegin();
			itr != aBB->m_listInstructions.rend();
			++itr)
		{
			i = 0;
			while (' ' != (*itr)[++i]);
			instruction = itr->substr(0, i);

			if ("mov" == instruction)
			{
				/* moving to `target`? */
				i = 3;
				while(' ' == (*itr)[++i]);
				j = i;
				while(',' != (*itr)[++j]);
				aTarget = itr->substr(i, j-i);

				if (target == aTarget)
				{
					/*
						Simple method: change the target and continue.
					*/
					target = itr->substr(j+1);
				}
			}
			else if (IsRegisterx86(target))
			{
				if ("or" == instruction)
				{
					/* could be `or reg,0xffffffff` */
					i = 3;
					while(' ' == (*itr)[++i]);

					j = i;
					while(',' != (*itr)[++j]);
					aTarget = itr->substr(i, j-i);

					if (target == aTarget && "0xffffffff" == itr->substr(j+1))
					{
						target = "0xffffffff";
					}
				}
				else if ("xor" == instruction)
				{
					/* could be `xor reg,reg` */
					i = 3;
					while(' ' == (*itr)[++i]);

					j = i;
					while(',' != (*itr)[++j]);
					aTarget = itr->substr(i, j-i);

					if (target == aTarget && target == itr->substr(j+1))
					{
						target = "0x0";
					}
				}
				else if ("eax" == target)
				{
					if ("call" == instruction)
					{
						if (reffile) {
							if (strstr(itr->c_str(), "DWORD PTR") ||
								*itr == "eax" || *itr == "ebx" || *itr == "ecx" || *itr == "edx" || *itr == "edi" || *itr == "esi" ||
								((*itr)[0] == '0' && (*itr)[0] == 'x'))
							{
								outref << MAGIC_FNPTR_CALL << endl;
								++indirectCalls;
							}
							else
							{
								outref << *itr << endl;
								++directCalls;
							}
						} else {
							cout << "new return value: " << *itr << endl;
						}
						retcall = 1;
						break;
					}
					else if ("int" == instruction && string::npos != itr->find("0x80"))
					{
						cout << "new return value: " << *itr << endl;
						retcall = 1;
						break;
					}
				}
			}
		}
		
		/* is this a literal? */
		if (target.size() > 2 && '0' == target[0] && 'x' == target[1])
		{
			cout << "new return value: " << target << " " << (int)strtoul(target.c_str(), NULL, 16) << endl;

			ReconstructPath(anElement, setVisited, targetBBs);
		}
		else if (!retcall && (head = graph->GetHead(anElement.bbId)))
		{
			anElement.bbIdParent = anElement.bbId;
			anElement.targetParent = anElement.target;

			anElement.target = target;
			do {
				anElement.bbId = head->m_bbIndex;
				if (setVisited.insert(anElement).second)
				{
					bfQueue.push(anElement);
				}
			} while (head = head->m_pNext);
		}
	}
}



void BFWalkSPARC(CFGraph* graph, int start, char* reffile)
{
	CFGraph::AdjListElement* head;
	queue<BFPathElement> bfQueue;
	
	BFPathElement anElement;

	BasicBlock* aBB;
	list<string>::const_reverse_iterator itr;
	set<BFPathElement> setVisited;
	ofstream outref;
	int directCalls = 0, indirectCalls = 0;
	
	string target, aTarget, instruction, op1, op2;

	int i, j;
	int retcall;

	if (reffile)
	{
		outref.open(reffile);
		if (!outref.is_open())
		{
			cerr << "Unable to open " << reffile << endl;
			reffile = NULL;
		}
	}

	if (head = graph->GetHead(start))
	{
		anElement.target = "%o0";
		do {
			anElement.bbId = head->m_bbIndex;
			bfQueue.push(anElement);
		} while (head = head->m_pNext);
	}
	while (!bfQueue.empty())
	{
		anElement = bfQueue.front();
		bfQueue.pop();

		target = anElement.target;

		aBB = graph->m_vBasicBlocks[anElement.bbId];

		retcall = 0;
		for (itr = aBB->m_listInstructions.rbegin();
			itr != aBB->m_listInstructions.rend();
			++itr)
		{
			i = 0;
			while (' ' != (*itr)[++i]);
			instruction = itr->substr(0, i);

			if ("mov" == instruction)
			{
				/* moving to `target`? */
				i = 3;
				while(' ' == (*itr)[++i]);
				j = i;
				while(',' != (*itr)[++j]);
				aTarget = itr->substr(i, j-i);
				
				i = j += 2;
				while((*itr)[j] != ' ' && (*itr)[j] != '\t' && j < itr->size()) ++j;
				if (target == itr->substr(i,j-i))
				{
					/*
						Simple method: change the target and continue.
					*/
					target = aTarget;
				}
			}
			else if (IsRegisterSPARC(target))
			{
				if ("orcc" == instruction)
				{
					/* could be `orcc  %g0, %r1, %r2` */
					i = 4;
					while(' ' == (*itr)[++i]);

					j = i;
					while(',' != (*itr)[++j]);
					op1 = itr->substr(i, j-i);

					i = j+=2;
					while(',' != (*itr)[++j]);
					op2 = itr->substr(i, j-i);

					aTarget = itr->substr(j+2);

					if (target == aTarget)
					{
						if (op1 == "%g0")
							target = op2;
						else if (op2 == "%g0")
							target = op1;
						if (op1 == "%g0" && op2 == "%g0") /* unlikely */
							target = "0x0";
					}
				}
				else if ("restore" == instruction)
				{
					if (target == "%o0")
						target = "%i0";
					if (itr->size() > 8)
					{
						/* could be `restore  %g0, %r1, %r2` */
						i = 7;
						while(' ' == (*itr)[++i]);

						j = i;
						while(',' != (*itr)[++j]);
						op1 = itr->substr(i, j-i);

						i = j+=2;
						while(',' != (*itr)[++j]);
						op2 = itr->substr(i, j-i);

						aTarget = itr->substr(j+2);
						if (target == "%i0" && aTarget == "%o0") // treat only this case as restore is the last instruction usually
						{										 // using %ix and %ox would be complete
							if (op1 == "%g0")
								target = op2;
							else if (op2 == "%g0")
								target = op1;
							if (op1 == "%g0" && op2 == "%g0") /* unlikely */
								target = "0x0";
						}
					}
				}
				else if ("clr" == instruction)
				{
					i = 4;
					while(' ' == (*itr)[++i]);
					j = i;
					while(',' != (*itr)[++j]);
					aTarget = itr->substr(i, j-i);

					if (target == aTarget)
						target = "0";

				}
				else if ("%o0" == target)
				{
					if ("call" == instruction)
					{
						if (reffile) {
							outref << *itr << endl;
						} else {
							cout << "new return value: " << *itr << endl;
						}
						retcall = 1;
						break;
					}
				}
			}
		}
		
		/* is this a literal? */
		if (target.size() > 0 && target[0] != '%' && target[0] != '[')
		{
			cout << "new return value: " << target << " " << target << endl;
		}
		else if (!retcall && (head = graph->GetHead(anElement.bbId)))
		{
			anElement.target = target;
			do {
				anElement.bbId = head->m_bbIndex;
				if (setVisited.insert(anElement).second)
				{
					bfQueue.push(anElement);
				}
			} while (head = head->m_pNext);
		}
	}
}


int main(int argc, char* argv[])
{
	CFGraph *graph, *tgraph;
	char *infile = "x.asm";
	char *reffile = NULL;
	CCFGBuilder builder;
	string asma;

	if (argc > 1)
		infile = argv[1];
	if (argc > 2)
		reffile = argv[2];

	asma = GetText(infile);
	if (asma.empty())
	{
		cerr << "Invalid or non-existing file: " << infile << endl;
		return -1;
	}
	graph = builder.Parse(asma.c_str());
	// cerr << "parsing done" << endl;

	CFGraph::AdjListElement* head;
	int i = 0;
	set<int> targetBBs;
	list<string>::const_iterator its;

	tgraph = new CFGraph();
	tgraph->m_vBasicBlocks = graph->m_vBasicBlocks;

	while (head = graph->GetHead(i))
	{
		for (;head; head = head->m_pNext)
		{
			// cout << "edge: " << i << " -> " << head->m_bbIndex << endl;
			tgraph->Add(i, head->m_bbIndex);
		}
		/*
		if (tgraph->m_vBasicBlocks[i])
		{
			for (its = (tgraph->m_vBasicBlocks[i])->m_listInstructions.begin();
				its != (tgraph->m_vBasicBlocks[i])->m_listInstructions.end(); ++its)
			{
				cout << *its << endl;
			}
		}
		*/
		++i;
	}
	char* fn = strrchr(infile, '/');
	if (!fn) fn = infile;
	else fn = fn + 1;

	ofstream outf;
	int a, b;

	BFWalkx86(tgraph, EXIT_NODE, targetBBs, reffile);
	
	/* preliminary support for counting side-effects functions in a particular
	 * library
	*/
/*
	if (a = BFSideEffectsx86(graph, 0, targetBBs))
	{
		outf.open("gse", ios::app);
		outf << fn << endl;
	}
	if (b = BFSideEffectsx862(graph, 0, targetBBs))
	{
		outf.open("argse", ios::app);
		outf << fn << endl;
	}

	if (!a && !b)
	{
		outf.open("nose", ios::app);
		outf << fn << endl;
	}
*/
	return 0;
}


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
};

bool operator<(const BFPathElement& lhs, const BFPathElement& rhs)
{
	if (lhs.bbId < rhs.bbId)
		return true;
	if (lhs.bbId == rhs.bbId && lhs.target < rhs.target)
		return true;
	
	return false;
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


void BFWalkx86(CFGraph* graph, int start, char* reffile)
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
								*itr == "eax" || *itr == "ebx" || *itr == "ecx" || *itr == "edx" || *itr == "edi" || *itr == "esi")
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

	CFGraph::AdjListElement* head;
	int i = 0;
	list<string>::const_iterator its;

	tgraph = new CFGraph();
	tgraph->m_vBasicBlocks = graph->m_vBasicBlocks;

	while (head = graph->GetHead(i))
	{
		for (;head; head = head->m_pNext)
		{
			tgraph->Add(i, head->m_bbIndex);
		}
		++i;
	}

	BFWalkx86(graph, 0, reffile);
	
	return 0;
}

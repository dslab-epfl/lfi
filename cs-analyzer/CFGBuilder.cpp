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

#include "CFGBuilder.h"

#include <set>
#include <map>
#include <string>
#include <iostream>
#include <string.h>
#include <stdlib.h>

using namespace std;

#define _CRT_SECURE_NO_DEPRECATE	1

CCFGBuilder::CCFGBuilder(void)
{
}

CCFGBuilder::~CCFGBuilder(void)
{
}
unsigned int GetLastOffset(const char* text)
{
	const char *tab1, *tab2, *lineend;
	unsigned int lastoffset, coffset;

	lastoffset = 0;
	lineend = text;

	do {
		tab1 = strchr(lineend, '\t');
		if (!tab1)
			break;

		coffset = strtoul(lineend, NULL, 16);
		if (coffset > lastoffset)
			lastoffset = coffset;

		lineend = strchr(lineend+1, '\n');
		
		tab2 = strchr(tab1+1, '\t');
		if (!tab2)
			break;
		/* handle null bytes in the instruction stream */
		if (tab2 > lineend)
			continue;
		++tab2;
	} while(tab1 && lineend);
	
	return lastoffset;
}

/*
	text should be of the form
	address:[ ]*\topcode[ ]*\tinstruction\n

	parsing is done in three phases:
		1. finding the callcount-th function call to targetfn
		2. determine jump targets within the code - O(n)
		3. linear pass and construct the CFG - O(n)
*/
CFGraph* CCFGBuilder::Parse(const char* text, const char* targetfn, int callcount, long int* callAddress)
{
	const char *tab1, *tab2, *lineend, *olineend;
	char address[16], instruction[256], jumptarget[16], buffer[256], function[256];
	set<string> jumptargets;
	unsigned int cjump, lastjumptarget, didjump, lastoffset, startaddress, endaddress;
	int indirectJumps, regularJumps;

	/* structures used to add the jxx-induced edges */
	map<string, int> startToBlockId; /* maps the jump targets to a block id (graph node id) */
	vector<int> fromBlockId;
	vector<string> toAddress;
	
	int i, j;

	lastjumptarget = 0;
	startaddress = 0;
	lastoffset = 0;
	endaddress = GetLastOffset(text);
	didjump = 0;
	indirectJumps = 0;
	regularJumps = 0;
	lineend = text;

	do {
		while (' ' == *lineend) ++lineend;

		*callAddress = strtoul(lineend, NULL, 16);

		tab1 = strchr(lineend, '\t');
		if (!tab1)
			break;

		lineend = strchr(lineend+1, '\n');
		
		tab2 = strchr(tab1+1, '\t');
		if (!tab2)
			break;
		/* handle null bytes in the instruction stream */
		if (tab2 > lineend)
			continue;
		++tab2;

		if (lineend)
		{
			if (lineend-tab2 < sizeof(instruction))
			{
				strncpy(instruction, tab2, lineend-tab2);
				instruction[lineend-tab2] = 0;
			} else {
				cerr << "what a long instruction: " << tab2 << endl;
				continue;
			}

			// cerr << instruction << endl;
			if (0 == strncmp(instruction, "call", 4))
			{
				/* determine the call target */
				i = 0;
				while (' ' != instruction[++i]);
				while (' ' == instruction[++i]);

				j = i;
				while (' ' != instruction[++j] && /* sanity check */ instruction[j] != 0);
				if (j-i < sizeof(jumptarget))
				{
					strncpy(jumptarget, &instruction[i], j-i);
					jumptarget[j-i] = 0;
					/* the call target can be many things. we're only interested in direct calls */
					i = j;
					while (' ' == instruction[i]) ++i;
					if (instruction[i] == '<') {
						j = ++i;
						while (instruction[j] != '@' && instruction[j] != '>' && /* sanity check */ instruction[j] != '\0') ++j;
						if (j-i < sizeof(function)) {
							strncpy(function, &instruction[i], j-i);
							function[j-i] = 0;
							if (0 == strcmp(function, targetfn))
								--callcount;
						}
					}
				} else {
					cerr << "jump target too big in " << instruction << endl;
				}
			}
		}
	} while(tab1 && lineend && callcount);

	if (callcount) /* no more calls to the function */
		return NULL;

	olineend = lineend;
	
	// find the jump targets to construct the CFG
	do {
		while (' ' == *lineend) ++lineend;
		if (!startaddress) {
			tab1 = strchr(lineend, ':');
			startaddress = strtoul(lineend, NULL, 16);
			endaddress = startaddress + 400; // semi-random, ~100 instructions are taken
		}
		tab1 = strchr(lineend, ':');
		if (!tab1)
			break;

		cjump = strtoul(lineend, NULL, 16);
		if (cjump >= endaddress)
		{
#ifdef DODEBUG
			cerr << "stopped parsing (again) at offset " << hex << cjump << endl;
#endif
			break;
		}

		tab1 = strchr(lineend, '\t');
		if (!tab1)
			break;

		lineend = strchr(lineend+1, '\n');
		tab2 = strchr(tab1+1, '\t');
		if (!tab2)
			break;

		/* handle null bytes in the instruction stream */
		if (tab2 > lineend)
			continue;

		++tab2;
		if (lineend)
		{
			if (lineend-tab2 < sizeof(instruction))
			{
				strncpy(instruction, tab2, lineend-tab2);
				instruction[lineend-tab2] = 0;
			} else {
				cerr << "what an instruction: " << tab2 << endl;
				continue;
			}
			
			if ('j' == instruction[0])
			{
				/* determine the jump target */
				i = 0;
				while (' ' != instruction[++i]);
				while (' ' == instruction[++i]);

				j = i;
				while (' ' != instruction[++j] && /* sanity check */ instruction[j] != 0);
				strncpy(jumptarget, &instruction[i], j-i);
				jumptarget[j-i] = 0;

				cjump = strtoul(jumptarget, NULL, 16);
				if (0 == strcmp(jumptarget, "eax") ||
					0 == strcmp(jumptarget, "ebx") ||
					0 == strcmp(jumptarget, "ecx") ||
					0 == strcmp(jumptarget, "edx") ||
					0 == strcmp(jumptarget, "esi") ||
					0 == strcmp(jumptarget, "edi"))
				{
					/* skip it. there's nothing we can do right now */
				} else {
					jumptargets.insert(jumptarget);
				}
			}
			++lineend;
		}
	} while(tab1 && lineend);
	// done finding the jump targets


	lineend = olineend;

	/* and the second pass */
	int cut, ret, prev, jmp;
	// lineend = text;
	cut = 0;
	ret = 0;
	jmp = 0;
	prev = 0;
	BasicBlock* currentBB = new BasicBlock();
	CFGraph* graph = new CFGraph();
	/* node 0 is the virtual start node - no instructions */
	graph->m_vBasicBlocks.push_back(new BasicBlock());

	do {
		while (' ' == *lineend) ++lineend;

		tab1 = strchr(lineend, ':');
		if (!tab1)
			break;

		cjump = strtoul(lineend, NULL, 16);
		if (cjump >= endaddress)
		{
#ifdef DODEBUG
			cerr << "stopped parsing (again) at offset " << hex << cjump << endl;
#endif
			break;
		}

		if (tab1-lineend < sizeof(address))
		{
			strncpy(address, lineend, tab1-lineend);
			address[tab1-lineend] = 0;
		}
		else {
			lineend = strchr(lineend+1, '\n');
			continue;
		}

		tab1 = strchr(lineend, '\t');

		lineend = strchr(lineend+1, '\n');

		tab2 = strchr(tab1+1, '\t');
		if (!tab2)
			break;
		/* handle null bytes in the instruction stream */
		if (tab2 > lineend)
			continue;

		++tab2;
		if (lineend)
		{
			if (cut || ret || jumptargets.find(address) != jumptargets.end())
			{
				/* if a ret was encountered add an extra edge to -1 (exit node) */
				if (ret)
				{
					graph->Add(EXIT_NODE, prev+1);
				}
				if (!jmp)
					graph->Add(prev+1, prev);

				++prev;
				graph->m_vBasicBlocks.push_back(currentBB);
				currentBB = new BasicBlock();
				if (!currentBB)
				{
					cerr << "Out of memory!" << endl;
					exit(-1);
				}

				if (jumptargets.find(address) != jumptargets.end())
				{
					startToBlockId.insert(map<string, int>::value_type(address, prev+1));
				}
				if (ret || (cut && 'j' == instruction[0] && 'm' == instruction[1] && 'p' == instruction[2]))
				{
					jmp = 1;
				}
				else
				{
					jmp = 0;
				}
				cut = ret = 0;
			}

			strncpy(instruction, tab2, lineend-tab2);
			instruction[lineend-tab2] = 0;
			
			if ('j' == instruction[0])
			{
				/* determine the jump target */
				i = 0;
				while (' ' != instruction[++i]);
				while (' ' == instruction[++i]);

				j = i;
				while (' ' != instruction[++j] && /* sanity check */ instruction[j] != 0);
				strncpy(jumptarget, &instruction[i], j-i);
				jumptarget[j-i] = 0;

				cjump = strtoul(jumptarget, NULL, 16);
				if (0 == strcmp(jumptarget, "eax") ||
					0 == strcmp(jumptarget, "ebx") ||
					0 == strcmp(jumptarget, "ecx") ||
					0 == strcmp(jumptarget, "edx") ||
					0 == strcmp(jumptarget, "esi") ||
					0 == strcmp(jumptarget, "edi"))
				{
					/* skip it. there's nothing we can do right now */
					++indirectJumps;
				}
				/*
					check for tail call
					replace jmp with call + ret
				*/
				else if (cjump < startaddress || cjump > endaddress)
				{
					/* jump goes outside the chunk of code we analyze.
					   discarding... */
					
					/* ret = 1; */
					cut = 1;
				} else {
#ifdef DODEBUG
					cerr << "found a jump to " << jumptarget << endl;
#endif
					++regularJumps;
					fromBlockId.push_back(prev+1);
					toAddress.push_back(jumptarget);
					cut = 1;
				}
			}
			else if (0 == strncmp(instruction, "ret", 3) || /* handle a gcc optimization */ 0 == strncmp(instruction, "repz ret", 8))
			{
				ret = 1;
			}
			
			currentBB->m_listInstructions.push_back(instruction);
			++lineend;
		}
	} while(tab1 && lineend);
	
	/* if a ret is encountered add an extra edge to the exit node */
	if (ret)
	{
		graph->Add(EXIT_NODE, prev+1);
	}
	if (!jmp)
	{
		graph->Add(prev+1, prev);
	}
	graph->m_vBasicBlocks.push_back(currentBB);


	/* add the jxx-induced edges */
	map<string, int>::iterator it;
	vector<int>::const_iterator it1;
	vector<string>::const_iterator it2;
	for (it1 = fromBlockId.begin(), it2 = toAddress.begin();
		it1 != fromBlockId.end() && it2 != toAddress.end();
		++it1, ++it2)
	{
		it = startToBlockId.find(*it2);
		if (it != startToBlockId.end())
		{
			graph->Add(it->second, *it1);
		}
		else
		{
			/* BUG. should never happen */
		}
	}
	/* cerr << regularJumps << ";" << indirectJumps << ";" << endl; */
	return graph;
}

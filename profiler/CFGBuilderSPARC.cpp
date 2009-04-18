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


/* support for SPARC shared objects is still in beta */

#include "CFGBuilderSPARC.h"

#include <set>
#include <map>
#include <string>
#include <iostream>

using namespace std;

#define _CRT_SECURE_NO_DEPRECATE	1
#define DODEBUG

CCFGBuilderSPARC::CCFGBuilderSPARC(void)
{
}

CCFGBuilderSPARC::~CCFGBuilderSPARC(void)
{
}

int CCFGBuilderSPARC::IsBranchInstruction(char* instruction)
{
	char jumps[][8] = { "b", "ba", "bn", "bu", "bg", "bug", "bl", "bul", "blg", "bne", "be", "bue", "bge", "buge", "ble", "bule", "bo", "bcc", "bleu", "brz", "brlez", "brlz", "brgez", "brnz", "brgz",
	"bcs", "bpos", "bneg", "bvc", "bvs", "bpa", "bpn", "bpne", "bpe", "bpg", "bple", "bpge", "bpl", "bpgu", "bpleu", "bpcc", "bpcs", "bppos", "bpneg", "bpvc", "bpvs"};
	int i;

	for (i = 0; i < sizeof(jumps)/sizeof(jumps[0]); ++i)
		if (0 == strncmp(instruction, jumps[i], strlen(jumps[i])) && (' ' == instruction[strlen(jumps[i])] || ',' == instruction[strlen(jumps[i])]))
			return 1;
	return 0;
}


unsigned int GetLastOffsetSPARC(const char* text)
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

	parsing is done in two phases:
		1. determine jump targets within the code - O(n)
		2. linear pass and construct the CFG - O(n)
*/
CFGraph* CCFGBuilderSPARC::Parse(const char* text)
{
	const char *tab1, *tab2, *lineend;
	char address[16], instruction[256], branch_instruction[256], jumptarget[16], buffer[256];
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
	endaddress = GetLastOffsetSPARC(text);
	didjump = 0;
	indirectJumps = 0;
	regularJumps = 0;
	lineend = text;


	do {
		tab1 = strchr(lineend, '\t');
		if (!tab1)
			break;

		lastoffset = strtoul(lineend, NULL, 16);
		if (!startaddress)
		{
			startaddress = lastoffset;
#ifdef DODEBUG
			cerr << "last off: " << hex << startaddress << " " << endaddress << endl;
#endif
		}
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

			if (IsBranchInstruction(instruction) || 0 == strncmp(instruction, "call", 4))
			{
				/* determine the jump target */
				i = 0;
				while (' ' != instruction[++i]);
				while (' ' == instruction[++i]);

				j = i;
				while (' ' != instruction[++j] && /* sanity check */ instruction[j] != 0);
				if (j-i < sizeof(jumptarget))
				{
					strncpy(jumptarget, &instruction[i], j-i);
					jumptarget[j-i] = 0;
					if (jumptarget[0] != '%')
					{
						cjump = strtoul(jumptarget, NULL, 16);
						if (cjump >= startaddress && cjump <= endaddress)
						{
							if (cjump > lastjumptarget)
								lastjumptarget = cjump;
							jumptargets.insert(jumptarget);
						} else {
							if (strncmp(instruction, "ba", 3) && strncmp(instruction, "call", 4))
								cerr << "WARNING: tail call detected in non-ba instruction (" << instruction << "). Is this the entire function?" << endl;
						}
					}
				}
				else
				{
					cerr << "what a jump target: " << instruction << " !" << endl;
				}
			}

			if (0 == strncmp(instruction, "ret", 3))
			{
				/* yes, ge */
				if (lastoffset >= lastjumptarget)
				{
#ifdef DODEBUG
					cerr << "stopped parsing at offset " << hex << lastoffset << endl;
#endif
					/* also process the instruction immediately after this due to SPARC's pipelined execution */
					lastjumptarget = lastoffset+5;
					break;
				}
			}
		}
	} while(tab1 && lineend);

	if (!lastjumptarget)
	{
		cerr << "WARNING: no jump or ret instructions encountered. Is this the entire function?" << endl;
		lastjumptarget = 0xffffffff;
	}

	/* and the second pass */
	int cut, ret, prev, jmp, ignore_current, delay_slot;
	lineend = text;
	cut = 0;
	ret = 0;
	jmp = 0;
	prev = 0;
	delay_slot = 0;
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
		if (cjump >= lastjumptarget)
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
			ignore_current = 0;
			if (cut || ret || jumptargets.find(address) != jumptargets.end())
			{
				if (delay_slot)
				{
					ignore_current = 1;

					strncpy(instruction, tab2, lineend-tab2);
					instruction[lineend-tab2] = 0;
					currentBB->m_listInstructions.push_back(instruction);
					currentBB->m_listInstructions.push_back(branch_instruction);
					delay_slot = 0;
				}

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
					/* unlikely that the instruction after a branch is a jump target */
					/* however, if it is, it will be copyied to both BBs */
					startToBlockId.insert(map<string, int>::value_type(address, prev+1));
				}
				if (ret || (cut && 'b' == instruction[0] && 'a' == instruction[1]))
				{
					/* TODO calls can be jmps in SPARC */
					jmp = 1;
				}
				else
				{
					jmp = 0;
				}
				cut = ret = 0;
			}

			if (!ignore_current) {
				strncpy(instruction, tab2, lineend-tab2);
				instruction[lineend-tab2] = 0;
				
				delay_slot = 1;
				if (IsBranchInstruction(instruction))
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
					if (jumptarget[0] == '%')
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
						sprintf(buffer, "call   %s", &instruction[i]);
						currentBB->m_listInstructions.push_back(buffer);
						strcpy(instruction, "ret");
						ret = 1;
					} else {
						++regularJumps;
						fromBlockId.push_back(prev+1);
						toAddress.push_back(jumptarget);
						cut = 1;
					}
				}
				else if (0 == strncmp(instruction, "call", 4))
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
					if (cjump > startaddress && cjump <= endaddress)
					{
						/* actually a jump */
						fromBlockId.push_back(prev+1);
						toAddress.push_back(jumptarget);
						cut = 1;
						/* discard %o7 for now */
						sprintf(instruction, "ba   %s", &instruction[i]);
					}

				}
				else if (0 == strncmp(instruction, "ret", 3))
				{
					ret = 1;
				}
				else
				{
					delay_slot = 0;
				}

				if (delay_slot)
				{
					strcpy(branch_instruction, instruction);
				}
				else
				{
					currentBB->m_listInstructions.push_back(instruction);
				}
			}
			++lineend;
		}
	} while(tab1 && lineend);
	
	/* if a ret is encountered add an extra edge to EXIT_NODE */
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
	return graph;
}

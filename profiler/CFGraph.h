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

#pragma once

#include <vector>
#include "BasicBlock.h"

using namespace std;

#define EXIT_NODE	499

class CFGraph
{
public:
	struct AdjListElement {
		int m_bbIndex;
		AdjListElement* m_pNext;
	};
public:
	CFGraph(void);
	~CFGraph(void);

	void Add(int what, int where);
	AdjListElement* GetHead(int where);
	vector<BasicBlock*> m_vBasicBlocks;
private:
	AdjListElement m_vectAdjLists[EXIT_NODE+1];
};

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

#include "CFGraph.h"
#include <iostream>
#include <string.h>

CFGraph::CFGraph(void)
{
	memset(m_vectAdjLists, 0, sizeof(m_vectAdjLists));
}

CFGraph::~CFGraph(void)
{
}

void CFGraph::Add(int what, int where)
{
	CFGraph::AdjListElement* newel = new CFGraph::AdjListElement();

	newel->m_bbIndex = what;
	newel->m_pNext = m_vectAdjLists[where].m_pNext;
	m_vectAdjLists[where].m_pNext = newel;
}

CFGraph::AdjListElement* CFGraph::GetHead(int where)
{
	return m_vectAdjLists[where].m_pNext;
}

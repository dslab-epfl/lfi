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

#include "StateTrigger.h"
#include <string.h>
#include <execinfo.h>
#include <iostream>
#ifdef __APPLE__
#include <pthread.h>
#endif

#ifdef __x86_64__
	#define REG_BP	"rbp"
#else
	#define REG_BP "ebp"
#endif

using namespace std;
void StateTrigger::Init(xmlNodePtr initData)
{
	xmlNodePtr nodeElement, nodeElementLvl2, textElement;

	nodeElement = initData->children;
	while (nodeElement)
	{
		if (XML_ELEMENT_NODE == nodeElement->type &&
		    (!xmlStrcmp(nodeElement->name, (const xmlChar*)"local") || !xmlStrcmp(nodeElement->name, (const xmlChar*)"global")))
		{
			var.offset = 0;
			var.frame = 1;
			var.location = (!xmlStrcmp(nodeElement->name, (const xmlChar*)"local") ? VAR_LOCAL : VAR_GLOBAL );

			nodeElementLvl2 = nodeElement->children;
			while (nodeElementLvl2)
			{
				textElement = nodeElementLvl2->children;
				if (textElement && XML_TEXT_NODE == textElement->type)
				{
					if (!xmlStrcmp(nodeElementLvl2->name, (const xmlChar*)"offset"))
						var.offset = (char*)strtoul((char*)textElement->content, NULL, 0);
					else if (!xmlStrcmp(nodeElementLvl2->name, (const xmlChar*)"type")) {
						if (!xmlStrcmp(textElement->content, (const xmlChar*)"int")) {
						  var.type = VAR_INT;
						} else if (!xmlStrcmp(textElement->content, (const xmlChar*)"string")) {
						  var.type = VAR_STRING;
						} else {
						  cerr << "[StateTrigger] Unknown variable type: " << (char*)nodeElementLvl2->content << endl;
						}
					} else if (!xmlStrcmp(nodeElementLvl2->name, (const xmlChar*)"value")) {
						if (VAR_INT == var.type)
						  var.targetValue.targetInt = atoi((char*)textElement->content);
						else if (VAR_STRING == var.type) {
						  // XXX - check string size
						  strcpy(var.targetValue.targetString, (char*)textElement->content);
						}
					} else if (!xmlStrcmp(nodeElementLvl2->name, (const xmlChar*)"frame")) {
						  var.frame = atoi((char*)textElement->content);
					} else if (!xmlStrcmp(nodeElementLvl2->name, (const xmlChar*)"value")) {
					        var.frame = atoi((char*)textElement->content);
					}
				}
				nodeElementLvl2 = nodeElementLvl2->next;
			}
			break;
		}
		nodeElement = nodeElement->next;
	}
}

struct layout
{
  struct layout *bp;
  void *ret;
  // void *args[8];
};

#ifdef __APPLE__
void *__libc_stack_end;
#else
extern void *__libc_stack_end;
#endif

bool StateTrigger::Eval(const string&, ...)
{
	int i;
#ifdef __APPLE__
	__libc_stack_end = pthread_get_stackaddr_np(pthread_self());
#endif
	if (VAR_GLOBAL == var.location) {
		if (VAR_INT == var.type) return (var.targetValue.targetInt == *(int*)var.offset);
		return !strcmp(var.targetValue.targetString, (char*)var.offset);
	}

	register void *bpx __asm__ ( REG_BP );

	struct layout *bp = (struct layout *)bpx;
	
	for (i = (int)var.frame+2; i; --i) {
		if ((void*)bp > __libc_stack_end || ((long)bp & 3))
			return false;
		bp = bp->bp;
	}
	if (VAR_INT == var.type) { 
		if (var.targetValue.targetInt == *(int*)((char*)bp + (long)var.offset))
			return true;
		return false;
	}
	char* target = *(char**)((char*)(bp) + (long)var.offset);
	if (!strcmp(var.targetValue.targetString, target))
		return true;
	return false;
}

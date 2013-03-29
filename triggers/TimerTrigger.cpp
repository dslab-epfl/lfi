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

#include "TimerTrigger.h"
#include <iostream>

StartTime TimerTrigger::start;

StartTime::StartTime()
{
  st_time = (unsigned)time(NULL);
}

TimerTrigger::TimerTrigger()
  : wait(0)
  , go(0)
{
}

void TimerTrigger::Init(xmlNodePtr initData)
{
  xmlNodePtr nodeElement, textElement;

  nodeElement = initData->children;
  while (nodeElement)
  {
    if (XML_ELEMENT_NODE == nodeElement->type &&
        !xmlStrcmp(nodeElement->name, (const xmlChar*)"wait"))
    {
      textElement = nodeElement->children;
      if (XML_TEXT_NODE == textElement->type)
        wait = atoi((char*)textElement->content);
    }
    nodeElement = nodeElement->next;
  }
}

bool TimerTrigger::Eval(const string&, ...)
{
  if (go)
    return true;

  if ((unsigned)time(NULL) - start.st_time >= wait) {
    go = 1;
  }

  if (go)
    return true;

  return false;
}

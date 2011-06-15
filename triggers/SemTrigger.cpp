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

#include "SemTrigger.h"
#include <iostream>
#include <stdarg.h>
#include <execinfo.h>

__thread int SemTrigger::lockCount = 0;

SemTrigger::SemTrigger()
{
}

bool SemTrigger::Eval(const string& functionName, ...)
{
	if (functionName == "pthread_mutex_lock")
	{
		++lockCount;
	}
	else if (functionName == "pthread_mutex_unlock")
	{
		if (lockCount) // sanity check
			--lockCount;
	}
	else
	{
		if (lockCount > 0)
			return true;
	}
	return false;
}

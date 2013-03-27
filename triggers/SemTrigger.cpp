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

#ifdef __APPLE__
pthread_key_t SemTrigger::lockCount_key = 0;
#else
__thread long SemTrigger::lockCount = 0;
#endif

SemTrigger::SemTrigger()
{
#ifdef __APPLE__
	if (!lockCount_key)
		pthread_key_create(&lockCount_key, NULL);
#endif
}

long SemTrigger::get_lockCount()
{
#ifdef __APPLE__
	return (long)pthread_getspecific(lockCount_key);
#else
	return lockCount;
#endif
}

void SemTrigger::set_lockCount(long v)
{
#ifdef __APPLE__
	pthread_setspecific(lockCount_key, (void*)v);
#else
	lockCount = v;
#endif
}


bool SemTrigger::Eval(const string& functionName, ...)
{
	long l;
	if (functionName == "pthread_mutex_lock")
	{
		set_lockCount(get_lockCount()+1);
	}
	else if (functionName == "pthread_mutex_unlock")
	{
		if (l = get_lockCount()) // sanity check
			set_lockCount(l - 1);
	}
	else
	{
		if (get_lockCount() > 0)
			return true;
	}
	return false;
}

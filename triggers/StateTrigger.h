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

#include "../Trigger.h"

enum VarType { VAR_INT, VAR_STRING };
enum VarLocation { VAR_GLOBAL, VAR_LOCAL };

struct Variable
{
	VarType type;
	VarLocation location;
	void* offset;
	int frame;
	union {
	  int targetInt;
	  char targetString[128];
        } targetValue;

	bool resolved; /* unused */
	string module; /* unused */
	string fileName; /* unused */
	int lineNumber;  /* unused */
};

DEFINE_TRIGGER( StateTrigger )
{
public:
	StateTrigger() { };
	void Init(xmlNodePtr initData);
	bool Eval(const string& functionName, ...);
private:
	Variable var;
};

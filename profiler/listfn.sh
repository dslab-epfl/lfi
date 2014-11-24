#!/bin/bash

#    Created by Paul Marinescu and George Candea
#    Copyright (C) 2009 EPFL (Ecole Polytechnique Federale de Lausanne)
#
#    This file is part of LFI (Library-level Fault Injector).
#
#    LFI is free software: you can redistribute it and/or modify it  
#    under the terms of the GNU General Public License as published by the  
#    Free Software Foundation, either version 3 of the License, or (at  
#    your option) any later version.
#
#    LFI is distributed in the hope that it will be useful, but  
#    WITHOUT ANY WARRANTY; without even the implied warranty of  
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  
#    General Public License for more details.
#
#    You should have received a copy of the GNU General Public  
#    License along with LFI. If not, see http://www.gnu.org/licenses/.
#
#    EPFL
#    Dependable Systems Lab (DSLAB)
#    Room 330, Station 14
#    1015 Lausanne
#    Switzerland



#profiles a entire library or a specific function
#disassembly must already be available on disk in the ./disassembly folder
#for the target function and all its dependencies. Output is an XML description
#use dasmfn.sh to generate the disassembly for an entire library


set +x

if [[ x$1 == x ]]
then
	echo "Usage: listfn libname.so <function name>"
	exit 1
fi

if [[ x$2 == x ]]; then
	TARGETFN="FUNC"
else
	TARGETFN="$2"
fi

readelf -s --wide $1 | grep FUNC > exports.tmp
FUNCTIONS=`cat exports.tmp |grep "$TARGETFN" | grep -v UND | awk '{ print $8 }'| grep -v '^_' | sed 's/@@.*//g'`

echo "<profile>"
for function in $FUNCTIONS
do

./profilermgr "$function" "$1" # 1>/dev/null 2>&1

echo "<function name=\"$function\">"
sort -u "profiles/$function" | awk '{print $5;}' | awk -f toxml.awk
echo "</function>"

rm -r -f profiles
mkdir profiles

done
echo "</profile>"
rm exports.tmp
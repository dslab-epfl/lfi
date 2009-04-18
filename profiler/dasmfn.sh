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

#disassembles the function specified as the second argument to disassembly/function_name
#if no function is specified, disassembles all the FUNC symbols found in the library

set +x

if [[ x$1 == x ]]
then
	echo "Usage: dasmfn libname.so <function name>"
	exit 1
fi

if [[ x$2 == x ]]; then
	TARGETFN="FUNC"
else
	TARGETFN=" $2$"
fi

readelf -s --wide $1 | grep FUNC > exports.tmp
cat exports.tmp |  awk '{ print "0x" $2 }'| sort > exports_sort.tmp
FUNCTIONS=`cat exports.tmp |grep "$TARGETFN" | grep -v UND | awk '{ print $8 }'` # | sed 's/@/_/g'`

for function in $FUNCTIONS
do

GARG=" $function$"

STARTOFFSET=0x`cat exports.tmp | grep -v '^_' | grep "$GARG" | awk '{ print $2 }' | head -1`

CHCNT=`echo $STARTOFFSET|wc -m`


if [ "$CHCNT" = "11" ]; then
cleanfunction=`echo $function|sed 's/@@.*//g'`

STOPOFFSET=`cat exports_sort.tmp | awk ' { if ( ( $0 ) > ("'$STARTOFFSET'") ) { print $0 ; exit } }'`
objdump -d -M intel --start-address=$STARTOFFSET --stop-address=$STOPOFFSET $1 | egrep -v "(^Disassembly|^/|^$)" > "disassembly/$cleanfunction"

echo $function $STARTOFFSET $STOPOFFSET
fi

done

rm exports_sort.tmp
rm exports.tmp

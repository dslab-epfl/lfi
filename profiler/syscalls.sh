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


#get syscall errors from kernel disassembly
#disassembly must already be available on disk in the ./disassembly folder
#for all syscalls and their dependecies
#use dasmfn.sh to generate the disassembly for the entire kernel

set +x

if [[ x$1 == x ]]
then
	echo "Usage: syscalls.sh <path to /arch/i386/kernel/syscall_table.S>"
	exit 1
fi

FUNCTIONS=`cat $1 |grep long | awk '{ print $2 }'`

echo "#include \"syscall_errors_head.h\""
for function in $FUNCTIONS
do

echo "int $function""_errors[] = {"

./profilermgr "$function" 1>/dev/null 2>&1


sort -u "profiles/$function" | awk '{if (0 != $5) { print $5 "," };}'
echo "0"
echo "};"


rm -r -f profiles
mkdir profiles

done

echo "struct sys_errors profiler_errors[] = {"
SYSCALLNO=0
for function in $FUNCTIONS
do
echo "{$SYSCALLNO, $function""_errors },"
SYSCALLNO=`expr $SYSCALLNO + 1`
done

echo "};"
#!/bin/bash

#profiles a entire library or a specific function
#disassembly must already be available on disk in the ./disassembly folder
# for the target function and all its dependencies. output is a C file
#use dasmfn.sh to generate the disassembly for an entire library


set +x

if [[ x$1 == x ]]
then
	echo "Usage: ./listfn_eval.sh libname.so"
	exit 1
fi

if [[ x$2 == x ]]; then
	TARGETFN="FUNC"
else
	TARGETFN=" $2$"
fi

readelf -s --wide $1 | grep FUNC > exports.tmp
FUNCTIONS=`cat exports.tmp |grep "$TARGETFN" | grep -v UND | awk '{ print $8 }'| grep -v '^_'`

echo "#include \"std_errors_head.h\""
for function in $FUNCTIONS
do

echo "int $function""_errors[] = {"

./profilermgr "$function" 1>/dev/null 2>&1


sort -u "profiles/$function" | awk '{ print $5 "," }'
echo "12345"
echo "};"


rm -r -f profiles
mkdir profiles

done

echo "struct sys_errors errors_profiler[] = {"

for function in $FUNCTIONS
do
echo "{\"$function\", $function""_errors },"
done

echo "};"
#!/bin/bash

#get syscall errors from man pages (obsoleted by syscalls.sh)

set +x

if [[ x$1 == x ]]
then
	echo "Usage: syscalls_errors /path/to/kernel/unistd.h"
	exit 1
fi


cat > test.xsl << EOF
<xsl:stylesheet version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:template match="/">
<xsl:for-each select="//errorcode">
<xsl:value-of select="."/>,
</xsl:for-each>
</xsl:template>
</xsl:stylesheet>
EOF


echo "#include \"syscall_errors_head.h\""

FUNCTIONS=`cat $1 | grep __NR_ | awk '{ print $2 }'| sed 's/__NR_//'`

for function in $FUNCTIONS
do
CLEANFUNC=`echo $function|sed 's/64//'|sed 's/32//'`
FILE=`find /usr/share/man/ -name "$CLEANFUNC.2.gz" | sort |head -n1`

if [[ x$FILE == "x" ]]
then
	#echo "unable to find man page for $function" 
	continue
fi

CALLID=`cat $1 | grep "__NR_"$function"\\>" | awk '{ print $3 }'`
if [[ $CALLID =~ ^[0-9]{1,3}$ ]]; then

cp $FILE  manpage.gz 
gunzip manpage.gz

if doclifter manpage 2> /dev/null 
then
echo "int "$function"_errors[] = { "
xsltproc test.xsl manpage.xml 2> /dev/null | grep -v version 2> /dev/null
echo "0 };"
echo
fi

rm -f manpage.xml
rm -f manpage

fi
done

rm -f test.xsl

echo "struct sys_errors errors_man[] = {"

for function in $FUNCTIONS
do
CLEANFUNC=`echo $function|sed 's/64//'|sed 's/32//'`

FILE=`find /usr/share/man/ -name "$CLEANFUNC.*.gz" | head -n1`

if [[ x$FILE == "x" ]]
then
	continue
fi

CALLID=`cat $1 | grep "__NR_"$function"\\>" | awk '{ print $3 }'`
if [[ $CALLID =~ ^[0-9]{1,3}$ ]]; then
echo "{"$CALLID, $function"_errors},"
fi
done

echo "};"

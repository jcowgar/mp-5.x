#!/bin/sh

echo "Configuring..."

echo "/* automatically created by config.sh - do not modify */" > config.h
>config.libs

# set compiler
[ "$CC" = "" ] && CC=cc

# add variables
cat VERSION >> config.h

# test for esound library
#echo "#include <esd.h>" > .tmp.c
#echo "int main(void) { return 0; }" >> .tmp.c
#
#$CC -I/usr/local/include -L/usr/local/lib -lesd .tmp.c -o /dev/null 2> /dev/null
#if [ $? = 0 ] ; then
#	echo "#define ESD 1" >> config.h
#	echo "-lesd" >> config.libs
#	echo "ESD enabled"
#fi

rm -f .tmp.c .tmp.o

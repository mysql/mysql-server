#! /bin/sh

# 1 - Dir
# 2 - Link dst

if [ $# -lt 1 ]
then
    exit 0
fi

files=`find $1 -type l -maxdepth 1`
res=$?
if [ $res -ne 0 ] || [ "$files" = "" ]
then
    exit 0
fi

rm -f $files




#! /bin/sh

# 1 - Link top src
# 2 - Link dst

if [ $# -lt 2 ]
then
    exit 0
fi

name=`basename $2`
files=`find $1/$name -type f -name '*.h*'`

for i in $files
do
    ln -s $i $2/`basename $i`
done




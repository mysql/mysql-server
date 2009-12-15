#!/bin/sh

set -e

mkdir -p result
cd result
rm -rf *

while [ $# -gt 0 ]
do
  rsync -a --exclude='BACKUP' --exclude='ndb_*_fs' "$1" .
  shift
done

#
# clean tables...not to make results too large
#
lst=`find . -name '*.frm'`
if [ "$lst" ]
then
    for i in $lst
    do
	basename=`echo $i | sed 's!\.frm!!'`
	if [ "$basename" ]
	then
	    rm -f $basename.*
	fi
    done
fi

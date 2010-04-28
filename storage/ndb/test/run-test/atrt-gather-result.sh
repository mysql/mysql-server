#!/bin/sh

set -e

mkdir -p result
cd result
rm -rf *

if uname | grep -iq cygwin; then
  while [ $# -gt 0 ]
  do
    SAVE_IFS=$IFS
    IFS=":"
    declare -a ARR=($1)
    IFS=$SAVE_IFS
    DIR=`dirname "${ARR[1]}"`
    REMOTE_DIR=`cygpath -u $DIR`
    HOST="${ARR[0]}"
    rsync -a --exclude='BACKUP' --exclude='ndb_*_fs' "$HOST:$REMOTE_DIR" .
    shift
  done
else
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
 	  basename=`echo $i | sed 's!\.frm!!'`
	  if [ "$basename" ]
	  then
	    rm -f $basename.*
	  fi
  fi

fi

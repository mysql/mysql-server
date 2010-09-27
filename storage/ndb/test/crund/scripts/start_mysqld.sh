#!/bin/bash
source ../env.properties
echo MYSQL_HOME=$MYSQL_HOME

echo
echo start mysqld...

mylogdir="ndblog"
mkdir -p "$mylogdir"

user="`whoami`"
cwd="`pwd`"
mycnf="$cwd/../my.cnf"
myerr="$cwd/$mylogdir/mysqld.log.err"
echo defaults-file=$mycnf
( cd $MYSQL_HOME ; "$MYSQL_BIN/mysqld_safe" --defaults-file="$mycnf" --user="$user" --log-error="$myerr" & )

# need some extra time
for ((i=0; i<3; i++)) ; do echo "." ; sleep 1; done

echo
echo show cluster...
"$MYSQL_BIN/ndb_mgm" -e show

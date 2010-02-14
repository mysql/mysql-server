#!/bin/bash
source env.properties
echo MYSQL_HOME=$MYSQL_HOME

echo
echo start mysqld...

cwd="`pwd`"
mycnf="$cwd/my.cnf"
myerr="$cwd/mysqld.log.err"
echo defaults-file=$mycnf
( cd $MYSQL_HOME ; "$MYSQL_BIN/mysqld_safe" --defaults-file="$mycnf" --user=mz --log-error="$myerr" & )

for ((i=0; i<3; i++)) ; do echo "." ; sleep 1; done

echo
echo show cluster...
"$MYSQL_BIN/ndb_mgm" -e show

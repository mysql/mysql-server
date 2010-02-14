#!/bin/bash
source env.properties
echo MYSQL_HOME=$MYSQL_HOME

echo shutdown mysqld...
"$MYSQL_BIN/mysqladmin" shutdown

for ((i=0; i<3; i++)) ; do echo "." ; sleep 1; done

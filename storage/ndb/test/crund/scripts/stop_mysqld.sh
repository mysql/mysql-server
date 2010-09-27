#!/bin/bash
source ../env.properties
echo MYSQL_HOME=$MYSQL_HOME

echo shut down mysqld...
"$MYSQL_BIN/mysqladmin" shutdown

# need some extra time
for ((i=0; i<3; i++)) ; do echo "." ; sleep 1; done

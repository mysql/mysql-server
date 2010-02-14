#!/bin/bash
source env.properties
echo MYSQL_HOME=$MYSQL_HOME

echo shutdown NDB...
"$MYSQL_BIN/ndb_mgm" -e shutdown

for ((i=0; i<10; i++)) ; do echo "." ; sleep 1; done

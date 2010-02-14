#!/bin/bash
source env.properties
echo MYSQL_HOME=$MYSQL_HOME

echo
echo start mgmd...
( cd ndblog ; "$MYSQL_LIBEXEC/ndb_mgmd" -f ../config.ini )

for ((i=0; i<2; i++)) ; do echo "." ; sleep 1; done

echo
echo start ndbd...
( cd ndblog ; "$MYSQL_LIBEXEC/ndbd" --initial )

for ((i=0; i<3; i++)) ; do echo "." ; sleep 1; done

echo
echo show cluster...
"$MYSQL_BIN/ndb_mgm" -e show

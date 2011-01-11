#!/bin/bash
source ../env.properties
echo MYSQL_HOME=$MYSQL_HOME

#set -x

mylogdir="ndblog"
mkdir -p "$mylogdir"

myini="../../config.ini"
echo
echo start mgmd...
( cd "$mylogdir" ; "$MYSQL_LIBEXEC/ndb_mgmd" --initial -f "$myini" )

# need some extra time
for ((i=0; i<3; i++)) ; do echo "." ; sleep 1; done

echo
echo start ndbd...
( cd "$mylogdir" ; "$MYSQL_LIBEXEC/ndbd" --initial )

#echo
#echo start ndbd...
#( cd "$mylogdir" ; "$MYSQL_LIBEXEC/ndbd" --initial )

# need some extra time
for ((i=0; i<1; i++)) ; do echo "." ; sleep 1; done

timeout=60
echo
echo waiting up to $timeout s for ndbd to start up...
"$MYSQL_BIN/ndb_waiter" -t $timeout

echo
echo show cluster...
"$MYSQL_BIN/ndb_mgm" -e show -t 1

#set +x

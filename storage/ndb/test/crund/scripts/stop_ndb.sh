#!/bin/bash
source ../env.properties
echo MYSQL_HOME=$MYSQL_HOME

echo shut down NDB...
"$MYSQL_BIN/ndb_mgm" -e shutdown -t 1

timeout=60
echo
echo waiting up to $timeout s for ndb_mgmd to shut down...
#"$MYSQL_BIN/ndb_waiter" -t $timeout -c "localhost:1186" --no-contact
"$MYSQL_BIN/ndb_waiter" -t $timeout --no-contact

# need some extra time
for ((i=0; i<6; i++)) ; do echo "." ; sleep 1; done

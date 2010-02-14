#!/bin/bash

./start_ndb.sh

# the ndb nodes seem to require some time to start
for ((i=0; i<30; i++)) ; do echo "." ; sleep 1; done

./start_mysqld.sh

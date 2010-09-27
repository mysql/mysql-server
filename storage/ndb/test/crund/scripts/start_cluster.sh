#!/bin/bash

./start_ndb.sh

# need some extra time
#for ((i=0; i<1; i++)) ; do echo "." ; sleep 1; done

./start_mysqld.sh

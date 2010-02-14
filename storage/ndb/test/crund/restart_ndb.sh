#!/bin/bash

./stop_ndb.sh

for ((i=0; i<2; i++)) ; do echo "." ; sleep 1; done

./start_ndb.sh

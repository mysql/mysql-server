#!/bin/bash

./stop_mysqld.sh

for ((i=0; i<2; i++)) ; do echo "." ; sleep 1; done

./start_mysqld.sh

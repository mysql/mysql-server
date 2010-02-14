#!/bin/bash

./stop_cluster.sh

for ((i=0; i<2; i++)) ; do echo "." ; sleep 1; done

./start_cluster.sh

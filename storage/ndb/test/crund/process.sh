#!/bin/bash
echo "running result processor..."
java -classpath build com.mysql.cluster.crund.ResultProcessor $*
echo "done."

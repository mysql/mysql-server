#!/bin/sh

NOTEFILE=`mktemp -t my_npm.XXXXX`
node setup/configure.js $NOTEFILE
if [ $? -eq 0 ]
  then 
    . $NOTEFILE
    node-waf configure --mysql=$PREFERRED_MYSQL
    node-waf clean build
fi
rm -f $NOTEFILE


#!/bin/sh

NOTEFILE=`mktemp -t npm.XXXXXX`
node setup/configure.js $NOTEFILE
conf_exit=$?
if [ $conf_exit = "0" ]
  then 
    . $NOTEFILE
    node-waf configure --mysql=$PREFERRED_MYSQL
    node-waf clean build
fi

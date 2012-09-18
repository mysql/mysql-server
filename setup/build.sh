#!/bin/sh

NOTEFILE=`mktemp -t npm.XXXXXX`
node setup/configure.js $NOTEFILE
conf_exit=$?
if [ $conf_exit = "0" ]
  then 
    source $NOTEFILE
    node-waf clean || true
    node-waf configure --mysql=$PREFERRED_MYSQL
fi

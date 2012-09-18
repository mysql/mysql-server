#!/bin/sh

node setup/configure.js
conf_exit=$?
if [ $conf_exit = "0" ]
  then 
    source setup/mysql_pref.sh
    node-waf configure --mysql=$PREFERRED_MYSQL
  else
    exit 1
fi

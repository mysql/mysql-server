#!/bin/sh

#
# This script is the "standard" way to configure MariaDB on Windows. To be 
# used by buildbot slaves and release build script.
# 

set -e

cscript win/configure.js --with-plugin-archive --with-plugin-blackhole \
 --with-plugin-csv --with-plugin-example --with-plugin-federatedx \
 --with-plugin-merge --with-plugin-partition --with-plugin-maria \
 --with-plugin-pbxt --with-plugin-xtradb --with-plugin-feedback \
 WITH_EMBEDDED_SERVER


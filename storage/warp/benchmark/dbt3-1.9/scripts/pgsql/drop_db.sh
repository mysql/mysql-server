#!/bin/sh

DIR=`dirname $0`
. ${DIR}/pgsql_profile || exit 1

/usr/bin/dropdb $SID

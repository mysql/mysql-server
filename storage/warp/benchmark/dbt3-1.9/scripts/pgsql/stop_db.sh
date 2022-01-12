#!/bin/bash

DIR=`dirname $0`
. ${DIR}/pgsql_profile || exit

# We only need to stop the database if it's running.
if [ -f ${PGDATA}/postmaster.pid ]; then
	sleep 1
	/usr/lib/postgresql/12/bin//pg_ctl -D ${PGDATA} stop ${1}
	sleep 1
fi

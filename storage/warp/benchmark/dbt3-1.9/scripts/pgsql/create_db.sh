#!/bin/sh

#
# This file is released under the terms of the Artistic License.
# Please see the file LICENSE, included in this package, for details.
#
# Copyright (C) 2002 Open Source Development Lab, Inc.
#

DIR=`dirname $0`
. ${DIR}/pgsql_profile || exit 1

# dont start script as root!
id=`id | sed s/\(.*// | sed s/uid=//`

if [ "$id" = "0" ]; then
	echo "dont start script as root"
	exit 1
fi

OUTPUT_DIR="."
while getopts "o:p:" OPT; do
	case ${OPT} in
	o)
		OUTPUT_DIR=${OPTARG}
		;;
	p)
		PARAMETERS=${OPTARG}
		;;
	esac
done

${SHELL} /home/justin/warp/storage/warp/benchmark/dbt3-1.9/scripts/pgsql/stop_db.sh

if [ -d $PGDATA ]; then
	echo "======================================="
	echo "PGData directory $PGDATA already exists"
	echo "Skipping initdb"
	echo "======================================="

	# Don't need to initdb, blindly try to drop the dbt3 database in case
	# it exists.
	${SHELL} /home/justin/warp/storage/warp/benchmark/dbt3-1.9/scripts/pgsql/start_db.sh -o ${OUTPUT_DIR} -p "$PARAMETERS" || exit 1
	${SHELL} /home/justin/warp/storage/warp/benchmark/dbt3-1.9/scripts/pgsql/drop_db.sh
else
	# initialize database cluster
	echo "initializing database cluster..."
	/usr/lib/postgresql/12/bin//initdb -D $PGDATA || exit 1
fi

${SHELL} /home/justin/warp/storage/warp/benchmark/dbt3-1.9/scripts/pgsql/start_db.sh -o ${OUTPUT_DIR} -p "$PARAMETERS" || exit

echo "Creating database $SID..."
_o=`/usr/bin/createdb $SID 2>&1`
_test=`echo $_o | grep CREATE`
if [ "$_test" = "" ]; then
	echo "/usr/bin/createdb $SID failed: $_o $_test"
	exit 1
fi

exit 0

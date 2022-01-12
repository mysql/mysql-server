#!/bin/bash

#
# This file is released under the terms of the Artistic License.
# Please see the file LICENSE, included in this package, for details.
#
# Copyright (C) 2005 Jenny Zhang & Open Source Development Labs, Inc.
#

DIR=`dirname $0`
. ${DIR}/pgsql_profile || exit 1

/usr/bin/psql -e -d $SID -c "DROP TABLE supplier;"
/usr/bin/psql -e -d $SID -c "DROP TABLE part;"
/usr/bin/psql -e -d $SID -c "DROP TABLE partsupp;"
/usr/bin/psql -e -d $SID -c "DROP TABLE customer;"
/usr/bin/psql -e -d $SID -c "DROP TABLE orders;"
/usr/bin/psql -e -d $SID -c "DROP TABLE lineitem;"
/usr/bin/psql -e -d $SID -c "DROP TABLE nation;"
/usr/bin/psql -e -d $SID -c "DROP TABLE region;"
/usr/bin/psql -e -d $SID -c "DROP TABLE time_statistics;"

exit 0

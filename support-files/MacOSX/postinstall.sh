#!/bin/sh
#
# postinstall - this script will be executed after the MySQL PKG
# installation has been performed.
#
# This script will install the MySQL privilege tables using the
# "mysql_install_db" script and will correct the ownerships of these files
# afterwards.
#

cd @prefix@
if [ ! -f data/mysql/db.frm ] ; then
	./scripts/mysql_install_db
fi

if [ -d data ] ; then
	chown -R @MYSQLD_USER@ data
fi

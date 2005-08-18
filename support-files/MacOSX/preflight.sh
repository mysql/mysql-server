#!/bin/sh
#
# preflight - this script will be executed before the MySQL PKG
# installation will be performed.
#
# If this package has been compiled with a prefix ending with "mysql" (e.g.
# /usr/local/mysql or /opt/mysql), it will rename any previously existing
# directory with this name before installing the new package (which includes
# a symlink named "mysql", pointing to the newly installed directory, which
# is named mysql-<version>)
#

PREFIX="@prefix@"
BASENAME=`basename $PREFIX`

if [ -d $PREFIX -a ! -L $PREFIX -a $BASENAME = "mysql" ] ; then
	mv $PREFIX $PREFIX.bak
fi

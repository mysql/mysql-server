#!/bin/sh
#
# (C)opyright Oracle/Innobase Oy. 2007
#
# Prerequisites: At the minimum rsync, auto{make, conf}, gcc, g++, perl
#
# Purpose: Build a dynamic plugin that can be distributed to users.
#
# Usage: This script takes at the minimum 4 parameters:
# 1.     the MySQL source directory,
#
# 2.     the plugin build directory - better if this doesn't exist,
#
# 3.     an SVN repository URL or path to a tar.gz file that contains
#        the plugin source. The tar file should be named such that the
#        top level directory in the archive is reflected in the name of
#        the tar file. e.g. innodb-5.1-1.0.b1.tar.gz when extracted should
#        have a top level directory named "innodb-5.1-1.0.b1".
#
# 4.     path to the target mysqlbug file or '-', if the third param is
#        '-' then all options following it are passed to the configure command.
#
# Note: The mysqlbug file is normally located in the bin directory where you
# will find the MySQL binaries. Remember to use the same mysqlbug file as the
# one used by the target version, run (grep '^VERSION=' mysqlbug) file to check.

set -eu

# Calculate the length of a string
strlen()
{
	STRLEN=`echo "$@" | wc -c | cut -c1-8`
	STRLEN=`expr $STRLEN - 1`
	echo $STRLEN
}

INNODIR="storage/innobase"
DYNTMPFILE="/tmp/configure.$$"
DYNCONFIG="$INNODIR/scripts/dynconfig"
SVN_REPO="https://svn.innodb.com/svn/innodb"
SVN_REPO_STRLEN=`strlen $SVN_REPO`

if [ $# -lt 4 ]; then
    echo>&2 "Usage: $0 mysql-source-dir build-dir innosrc (/path/to/mysqlbug | - followed by configure options)"
    exit 1
fi

SRC=$1; shift
BLD=$1; shift
SVN=$1; shift
CFL=$1; shift

# These can be overridden with environment variables.
# For example: MAKE="make -j4" or RSYNC="rsync -v"
: ${RSYNC="rsync --inplace"}
: ${MAKE="make"}
: ${SVN_CO="svn checkout -q"}

# TODO: exclude more
echo "Copying source from $SRC to $BLD ... "
$RSYNC	--exclude '*.c' --exclude '*.cc' --exclude 'storage/*/' \
	--delete-excluded -a "$SRC/" "$BLD/"
# the dependencies of include/mysqld_error.h
$RSYNC -a "$SRC"/strings "$SRC"/dbug "$SRC"/mysys "$BLD"
$RSYNC -a "$SRC"/extra/comp_err.c "$BLD"/extra/comp_err.c

cd "$BLD"
touch sql/mysqld.cc
rm -rf $INNODIR

# If we are building from the SVN repository then use svn tools
# otherwise the assumption is that we are dealing with a gzipped
# tarball.
REPO=${SVN:0:$SVN_REPO_STRLEN}
if [ "$REPO"x = "$SVN_REPO"x ]; then
	$SVN_CO "$SVN" $INNODIR
else
	(
		echo "Extracting source from tar file $SVN ..."
		cd `dirname $INNODIR`
		gunzip < $SVN | tar xf -
		mv `basename ${SVN%.t*z}` `basename $INNODIR`
	)
fi

echo "Creating Makefiles ..."
# Generate ./configure and storage/innobase/Makefile.in
#aclocal
#autoheader
#libtoolize --automake --force --copy
#automake --force --add-missing --copy
#autoconf

autoreconf --force --install

if [ "$CFL" != "-" ]; then

	if [ ! -f "$CFL" ]; then
		echo "$CFL not found!"
		exit 1
	fi

	if [ ! -f "$DYNCONFIG" ]; then
		echo "$DYNCONFIG not found!"
		exit 1
	fi

	trap "{ rm -f $DYNTMPFILE; }" EXIT SIGINT SIGTERM

	# Generate storage/innobase/Makefile and other prerequisites
	$DYNCONFIG $CFL > $DYNTMPFILE

	if [ $? -ne 0 ]; then
		echo "dynconfig failed to get config parameters: $CONFIGURE"
		exit 1
	fi

	# Now run the configure command
	chmod +x $DYNTMPFILE

	echo
	echo "***************************************************************"
	echo "Building plugin with " `grep '^VERSION=' $CFL` \
		" configure options"
	echo "***************************************************************"
	echo

	# Display the config parameters that will be used
	cat $DYNTMPFILE

	/bin/sh -c $DYNTMPFILE > /dev/null
else
	./configure "$@"
fi

(cd include; $MAKE my_config.h)

if [ ! -f include/mysqld_error.h ]; then
	echo "Generating include/mysqld_error.h ..."
	# Generate include/mysqld_error.h
	(cd strings; $MAKE)
	(cd dbug; $MAKE)
	(cd mysys; $MAKE)
	(cd extra; $MAKE ../include/mysqld_error.h)
fi

# Compile the InnoDB plugin.
cd $INNODIR
exec $MAKE

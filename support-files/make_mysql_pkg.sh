#!/bin/sh
#
# make_mysql_pkg.sh
#
# This script creates a Mac OS X installation package
# for Apple's Installer application.
#
# To use it:
# 
# 1.) unpack the MySQL source tarball
# 2.) cd to into the resulting directory and stay there for the next steps
# 3.) "configure" the source (preferably with --mandir=/usr/local/share/man)
# 4.) "make" the package
# 5.) invoke this script with superuser privileges (sudo or in a root shell)
#
# Written by Marc Liyanage (http://www.entropy.ch)
#
# History:
#
# When         Who              What
# -------------------------------------------------------------
# 2001-09-13   Marc Liyanage    First version


# Find the version number of this particular MySQL build
#
OLDWD=`pwd`
VERSION_H_FILE=$OLDWD/include/mysql_version.h

if [ ! -e $VERSION_H_FILE ]
then
echo $VERSION_H_FILE not found, make sure you are in the mysql source dir
exit 1
fi

MYSQLVERSION=`egrep 'MYSQL_SERVER_VERSION' $VERSION_H_FILE | perl -e '$_ = <>; $_ =~ /"(.+?)"/; print $1'`



# We will temporarily rename /usr/local to this name
# and then mkdir a new, empty /usr/local
#
LOCAL_TMPDIR=/usr/local.tmp

# At the end, we'll keep our temporary /usr/local
# to this name
#
LOCAL_BACKUPDIR=/usr/local.mysql-package

# Where do we create the package directory
#
PKG_DIR=/tmp/mysql-$MYSQLVERSION.pkg

# Where is the resources directory within the
# package directory
#
PKG_RESOURCES_DIR=$PKG_DIR/Contents/Resources

# Check if old stuff is in our way
#
if [ -e $LOCAL_BACKUPDIR ]
then
echo $LOCAL_BACKUPDIR exists, please remove first...
exit 1
fi

if [ -e $LOCAL_TMPDIR ]
then
echo $LOCAL_TMPDIR exists, please remove first...
exit 1
fi

if [ -e $PKG_DIR ]
then
echo $PKG_DIR exists, please remove first...
exit 1
fi

# Now create the package dir
#
mkdir -p $PKG_RESOURCES_DIR

# Move the existing /usr/local out of our way
#
mv /usr/local $LOCAL_TMPDIR

# Now create our new empty temporary /usr/local
#
mkdir /usr/local

# And install MySQL there
#
make install


# cd there so the next few commands will use it
# as base directory
#
cd /usr/local

# First, create the gzipped pax archive file
# which contains the actual files
#
pax -w . | gzip -c > $PKG_RESOURCES_DIR/mysql-$MYSQLVERSION.pax.gz

# Create the bom ("Bill Of Materials") file
#
mkbom . $PKG_RESOURCES_DIR/mysql-$MYSQLVERSION.bom

# Create the sizes file with the package space
# requirement numbers and file count
#
SIZE_UNCOMPRESSED=`du -sk /usr/local | cut -f 1`
SIZE_COMPRESSED=`du -sk $PKG_DIR | cut -f 1`
NUMFILES=`find /usr/local | wc -l | perl -e '$_ = <>; $_ =~ /\s+(\d+)/; print $1 - 1'`

echo NumFiles $NUMFILES >> $PKG_RESOURCES_DIR/mysql-$MYSQLVERSION.sizes
echo InstalledSize $SIZE_UNCOMPRESSED >> $PKG_RESOURCES_DIR/mysql-$MYSQLVERSION.sizes
echo CompressedSize $SIZE_COMPRESSED >> $PKG_RESOURCES_DIR/mysql-$MYSQLVERSION.sizes


# Finally create the info file which drives the "Installer" application
#
cat >$PKG_RESOURCES_DIR/mysql-$MYSQLVERSION.info <<- EOF 
	Title MySQL
	Version $MYSQLVERSION
	Description The MySQL database server in a convenient Mac OS X package. Some additional configuration is necessary, please see http://www.entropy.ch/software/macosx/mysql/
	DefaultLocation /usr/local
	Diskname (null)
	DeleteWarning
	NeedsAuthorization YES
	DisableStop NO
	UseUserMask NO
	Application NO
	Relocatable NO
	Required NO
	InstallOnly NO
	RequiresReboot NO
	InstallFat NO
EOF

# Create a .tar.gz file for the package directory
#
cd $PKG_DIR
cd ..
DIRNAME=`dirname $PKG_DIR`
BASENAME=`basename $PKG_DIR`
FILENAME=$BASENAME.tar.gz
tar -cvzf $FILENAME $BASENAME

# Move our temporary /usr/local out of the way
# and the original one back
#
mv /usr/local $LOCAL_BACKUPDIR
mv $LOCAL_TMPDIR /usr/local

echo output package is in $DIRNAME/$FILENAME



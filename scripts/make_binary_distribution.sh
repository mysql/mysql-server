#!/bin/sh
# Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

##############################################################################
#
#  This is a script to create a TAR or ZIP binary distribution out of a
#  built source tree. The output file will be put at the top level of
#  the source tree, as "mysql-<vsn>....{tar.gz,zip}"
#
#  Note that the structure created by this script is slightly different from
#  what a normal "make install" would produce. No extra "mysql" sub directory
#  will be created, i.e. no "$prefix/include/mysql", "$prefix/lib/mysql" or
#  "$prefix/share/mysql".  This is because the build system explicitly calls
#  make with pkgdatadir=<datadir>, etc.
#
#  In GNU make/automake terms
#
#    "pkglibdir"     is set to the same as "libdir"
#    "pkgincludedir" is set to the same as "includedir"
#    "pkgdatadir"    is set to the same as "datadir"
#    "pkgplugindir"  is set to "$pkglibdir/plugin"
#    "pkgsuppdir"    is set to "@prefix@/support-files",
#                    normally the same as "datadir"
#
#  The temporary directory path given to "--tmp=<path>" has to be
#  absolute and with no spaces.
#
#  Note that for best result, the original "make" should be done with
#  the same arguments as used for "make install" below, especially the
#  'pkglibdir', as the RPATH should to be set correctly.
#
##############################################################################

##############################################################################
#
#  Read the command line arguments that control this script
#
##############################################################################

machine=@MACHINE_TYPE@
system=@SYSTEM_TYPE@
SOURCE=`pwd`
CP="cp -p"
MV="mv"

# There are platforms, notably OS X on Intel (x86 + x86_64),
# for which "uname" does not provide sufficient information.
# The value of CFLAGS as used during compilation is the most exact info
# we can get - after all, we care about _what_ we built, not _where_ we did it.
cflags="@CFLAGS@"

STRIP=1				# Option ignored
SILENT=0
MALLOC_LIB=
PLATFORM=""
TMP=/tmp
NEW_NAME=""			# Final top directory and TAR package name
SUFFIX=""
SHORT_PRODUCT_TAG=""		# If don't want server suffix in package name
NDBCLUSTER=""			# Option ignored

for arg do
  case "$arg" in
    --tmp=*)    TMP=`echo "$arg" | sed -e "s;--tmp=;;"` ;;
    --suffix=*) SUFFIX=`echo "$arg" | sed -e "s;--suffix=;;"` ;;
    --short-product-tag=*) SHORT_PRODUCT_TAG=`echo "$arg" | sed -e "s;--short-product-tag=;;"` ;;
    --inject-malloc-lib=*) MALLOC_LIB=`echo "$arg" | sed -e 's;^[^=]*=;;'` ;;
    --no-strip) STRIP=0 ;;
    --machine=*) machine=`echo "$arg" | sed -e "s;--machine=;;"` ;;
    --platform=*) PLATFORM=`echo "$arg" | sed -e "s;--platform=;;"` ;;
    --silent)   SILENT=1 ;;
    --with-ndbcluster) NDBCLUSTER=1 ;;
    *)
      echo "Unknown argument '$arg'"
      exit 1
      ;;
  esac
done

# ----------------------------------------------------------------------
# Adjust "system" output from "uname" to be more human readable
# ----------------------------------------------------------------------

if [ x"$PLATFORM" = x"" ] ; then
  # FIXME move this to the build tools
  # Remove vendor from $system
  system=`echo $system | sed -e 's/[a-z]*-\(.*\)/\1/g'`

  # Map OS names to "our" OS names (eg. darwin6.8 -> osx10.2)
  system=`echo $system | sed -e 's/darwin6.*/osx10.2/g'`
  system=`echo $system | sed -e 's/darwin7.*/osx10.3/g'`
  system=`echo $system | sed -e 's/darwin8.*/osx10.4/g'`
  system=`echo $system | sed -e 's/darwin9.*/osx10.5/g'`
  system=`echo $system | sed -e 's/\(aix4.3\).*/\1/g'`
  system=`echo $system | sed -e 's/\(aix5.1\).*/\1/g'`
  system=`echo $system | sed -e 's/\(aix5.2\).*/\1/g'`
  system=`echo $system | sed -e 's/\(aix5.3\).*/\1/g'`
  system=`echo $system | sed -e 's/osf5.1b/tru64/g'`
  system=`echo $system | sed -e 's/linux-gnu/linux/g'`
  system=`echo $system | sed -e 's/solaris2.\([0-9]*\)/solaris\1/g'`
  system=`echo $system | sed -e 's/sco3.2v\(.*\)/openserver\1/g'`
fi

# Get the "machine", which really is the CPU architecture (including the size).
# The precedence is:
# 1) use an explicit argument, if given;
# 2) use platform-specific fixes, if there are any (see bug#37808);
# 3) stay with the default (determined during "configure", using predefined macros).

if [ x"$MACHINE" != x"" ] ; then
  machine=$MACHINE
else
  case $system in
    osx* )
      # Extract "XYZ" from CFLAGS "... -arch XYZ ...", or empty!
      cflag_arch=`echo "$cflags" | sed -n -e 's=.* -arch \([^ ]*\) .*=\1=p'`
      case "$cflag_arch" in
        i386 )    case $system in
                    osx10.4 )  machine=i686 ;; # Used a different naming
                    * )        machine=x86 ;;
                  esac ;;
        x86_64 )  machine=x86_64 ;;
        ppc )     ;;  # No treatment needed with PPC
        ppc64 )   ;;
        * ) # No matching compiler flag? "--platform" is needed
            if [ x"$PLATFORM" != x"" ] ; then
              :  # See below: "$PLATFORM" will take precedence anyway
            elif [ "$system" = "osx10.3" -a -z "$cflag_arch" ] ; then
              :  # Special case of OS X 10.3, which is PPC-32 only and doesn't use "-arch"
            else
              echo "On system '$system' only specific '-arch' values are expected."
              echo "It is taken from the 'CFLAGS' whose value is:"
              echo "$cflags"
              echo "'-arch $cflag_arch' is unexpected, and no '--platform' was given: ABORT"
              exit 1
            fi ;;
      esac  # "$cflag_arch"
      ;;
  esac  # $system
fi

# Combine OS and CPU to the "platform". Again, an explicit argument takes precedence.
if [ x"$PLATFORM" != x"" ] ; then
  :  
else
  PLATFORM="$system-$machine"
fi

# Print the platform name for build logs
echo "PLATFORM NAME: $PLATFORM"

# Change the distribution to a long descriptive name
# For the cluster product, concentrate on the second part
VERSION_NAME=@VERSION@
case $VERSION_NAME in
  *-ndb-* )  VERSION_NAME=`echo $VERSION_NAME | sed -e 's/[.0-9]*-ndb-//'` ;;
  *-MariaDB-* ) VERSION_NAME=`echo $VERSION_NAME | sed -e 's/-MariaDB//'` ;;
esac
if [ x"$SHORT_PRODUCT_TAG" != x"" ] ; then
  NEW_NAME=mariadb-$SHORT_PRODUCT_TAG-$VERSION_NAME-$PLATFORM$SUFFIX
else
  NEW_NAME=mariadb@MYSQL_SERVER_SUFFIX@-$VERSION_NAME-$PLATFORM$SUFFIX
fi

# ----------------------------------------------------------------------
# Define BASE, and remove the old BASE directory if any
# ----------------------------------------------------------------------
BASE=$TMP/my_dist$SUFFIX
if [ -d $BASE ] ; then
 rm -rf $BASE
fi

# ----------------------------------------------------------------------
# Find the TAR to use
# ----------------------------------------------------------------------

# This is needed to prefer GNU tar over platform tar because that can't
# always handle long filenames

PATH_DIRS=`echo $PATH | \
    sed -e 's/^:/. /' -e 's/:$/ ./' -e 's/::/ . /g' -e 's/:/ /g' `

which_1 ()
{
  for cmd
  do
    for d in $PATH_DIRS
    do
      for file in $d/$cmd
      do
        if [ -x $file -a ! -d $file ] ; then
          echo $file
          exit 0
        fi
      done
    done
  done
  exit 1
}

tar=`which_1 gnutar gtar`
if [ $? -ne 0 -o x"$tar" = x"" ] ; then
  tar=tar
fi


##############################################################################
#
#  Handle the Unix/Linux packaging using "make install"
#
##############################################################################

# ----------------------------------------------------------------------
# Terminate on any base level error
# ----------------------------------------------------------------------
set -e

#
# Check that the client is compiled with libmysqlclient.a
#
if test -f ./client/.libs/mysql
then
  echo ""
  echo "The MySQL clients are compiled dynamicly, which is not allowed for"
  echo "a MySQL binary tar file.  Please configure with"
  echo "--with-client-ldflags=-all-static and try again"
  exit 1;
fi

# ----------------------------------------------------------------------
# Really ugly, one script, "mysql_install_db", needs prefix set to ".",
# i.e. makes access relative the current directory. This matches
# the documentation, so better not change this. And for another script,
# "mysql.server", we make some relative, others not.
# ----------------------------------------------------------------------

cd scripts
rm -f mysql_install_db mysqld_safe mysql_fix_privilege_tables
@MAKE@ mysql_install_db mysqld_safe mysql_fix_privilege_tables \
  prefix=. \
  bindir=./bin \
  sbindir=./bin \
  scriptdir=./bin \
  libexecdir=./bin \
  pkgdatadir=./share \
  localstatedir=./data
cd ..

cd support-files
rm -f mysql.server
@MAKE@ mysql.server \
  bindir=./bin \
  sbindir=./bin \
  scriptdir=./bin \
  libexecdir=./bin \
  pkgdatadir=./share 
cd ..

# ----------------------------------------------------------------------
# Do a install that we later are to pack. Use the same paths as in
# the build for the relevant directories.
# ----------------------------------------------------------------------
@MAKE@ DESTDIR=$BASE install \
  libexecdir=@prefix@/libexec \
  pkglibdir=@pkglibdir@ \
  pkgincludedir=@pkgincludedir@ \
  pkgdatadir=@pkgdatadir@ \
  pkgplugindir=@pkgplugindir@ \
  pkgsuppdir=@pkgsuppdir@ \
  mandir=@mandir@ \
  infodir=@infodir@

# ----------------------------------------------------------------------
# Rename top directory, and set DEST to the new directory
# ----------------------------------------------------------------------
mv $BASE@prefix@ $BASE/$NEW_NAME
DEST=$BASE/$NEW_NAME

# ----------------------------------------------------------------------
# If we compiled with gcc, copy libgcc.a to the dist as libmygcc.a
# ----------------------------------------------------------------------
if [ x"@GXX@" = x"yes" ] ; then
  gcclib=`@CC@ @CFLAGS@ --print-libgcc-file 2>/dev/null` || true
  if [ -z "$gcclib" ] ; then
    echo "Warning: Compiler doesn't tell libgcc.a!"
  elif [ -f "$gcclib" ] ; then
    $CP $gcclib $DEST/lib/libmygcc.a
  else
    echo "Warning: Compiler result '$gcclib' not found / no file!"
  fi
fi

# If requested, add a malloc library .so into pkglibdir for use
# by mysqld_safe
if [ -n "$MALLOC_LIB" ]; then
  cp "$MALLOC_LIB" "$DEST/lib/"
fi

# Note, no legacy "safe_mysqld" link to "mysqld_safe" in 5.1

# Copy readme and license files
cp README Docs/INSTALL-BINARY  $DEST/
if [ -f COPYING -a -f EXCEPTIONS-CLIENT ] ; then
  cp COPYING EXCEPTIONS-CLIENT $DEST/
elif [ -f LICENSE.mysql ] ; then
  cp LICENSE.mysql $DEST/
else
  echo "ERROR: no license files found"
  exit 1
fi

# FIXME should be handled by make file, and to other dir
mkdir -p $DEST/bin $DEST/support-files
cp scripts/mysqlaccess.conf $DEST/bin/
cp support-files/magic      $DEST/support-files/

# Create empty data directories, set permission (FIXME why?)
mkdir       $DEST/data $DEST/data/mysql $DEST/data/test
chmod o-rwx $DEST/data $DEST/data/mysql $DEST/data/test

# Remove not needed files
rm $DEST/share/mysql/errmsg.txt

# Remove NDB files
rm -f $DEST/share/mysql/ndb-config-2-node.ini \
    $DEST/share/mysql/config*

#
# Move things to make them easier to find in tar installation
#

# The following test is needed if the original configure was done with
# something like --libexecdir=/usr/local/mysql/bin
if test -f $DEST/libexec/mysqld
then
  mv $DEST/libexec/* $DEST/bin
  rmdir $DEST/libexec
fi
mv $DEST/share/man $DEST
mv $DEST/share/mysql/binary-configure $DEST/configure
mv $DEST/share/mysql/*.sql $DEST/share
mv $DEST/share/mysql/*.cnf $DEST/share/mysql/*.server $DEST/share/mysql/mysql-log-rotate $DEST/support-files

#
# Move some scripts that are only run once to 'scripts' directory
# but add symbolic links instead to old place for compatibility
#
mkdir $DEST/scripts
for i in mysql_secure_installation mysql_fix_extensions mysql_fix_privilege_tables mysql_install_db mytop
do
  mv $DEST/bin/$i $DEST/scripts
  ln -s "../scripts/$i" $DEST/bin/$i
done

# ----------------------------------------------------------------------
# Create the result tar file
# ----------------------------------------------------------------------

echo "Using $tar to create archive"
OPT=cvf
if [ x$SILENT = x1 ] ; then
  OPT=cf
fi

echo "Creating and compressing archive"
rm -f $NEW_NAME.tar.gz
(cd $BASE ; $tar $OPT -  $NEW_NAME) | gzip -9 > $NEW_NAME.tar.gz
echo "$NEW_NAME.tar.gz created"

echo "Removing temporary directory"
rm -rf $BASE
exit 0


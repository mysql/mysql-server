#!/bin/sh
#
# Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
#
# Solaris post install script
#

#if [ /usr/man/bin/makewhatis ] ; then
#  /usr/man/bin/makewhatis "$BASEDIR/mysql/man"
#fi

mygroup=mysql
myuser=mysql
mydatadir=/var/lib/mysql
basedir=@@basedir@@

if [ -n "$BASEDIR" ] ; then
  basedir="$BASEDIR"
fi

# What MySQL calls "basedir" and what pkgadd tools calls "basedir"
# is not the same thing. The default pkgadd base directory is /opt/mysql,
# the MySQL one "/opt/mysql/mysql".

mybasedir="$basedir/@@instdir@@"
mystart1="$mybasedir/support-files/mysql.server"
myinstdb="$mybasedir/scripts/mysql_install_db"
mystart=/etc/init.d/mysql

# Check: Is this a first installation, or an upgrade ?

if [ -d "$mydatadir/mysql" ] ; then
  :   # If the directory for system table files exists, we assume an upgrade.
else
  INSTALL=new  # This is a new installation, the directory will soon be created.
fi

# Create data directory if needed

[ -d "$mydatadir"       ] || mkdir -p -m 755 "$mydatadir" || exit 1
[ -d "$mydatadir/mysql" ] || mkdir "$mydatadir/mysql"     || exit 1
[ -d "$mydatadir/test"  ] || mkdir "$mydatadir/test"      || exit 1

# Set the data directory to the right user/group

chown -R $myuser:$mygroup $mydatadir

# Solaris patch 119255 (somewhere around revision 42) changes the behaviour
# of pkgadd to set TMPDIR internally to a root-owned install directory.  This
# has the unfortunate side effect of breaking running mysql_install_db with
# the --user=mysql argument as mysqld uses TMPDIR if set, and is unable to
# write temporary tables to that directory.  To work around this issue, we
# create a subdirectory inside TMPDIR (if set) for mysqld to write to.
#
# Idea from Ben Hekster <heksterb@gmail.com> in bug#31164

if [ -n "$TMPDIR" ] ; then
  savetmpdir="$TMPDIR"
  TMPDIR="$TMPDIR/mysql.$$"
  export TMPDIR
  mkdir "$TMPDIR"
  chown $myuser:$mygroup "$TMPDIR"
fi

if [ -n "$INSTALL" ] ; then
  # We install/update the system tables
  (
    cd "$mybasedir"
    scripts/mysql_install_db \
	  --rpm \
	  --user=mysql \
	  --basedir="$mybasedir" \
	  --datadir=$mydatadir
  )
fi

if [ -n "$savetmpdir" ] ; then
  TMPDIR="$savetmpdir"
fi

# ----------------------------------------------------------------------

# Handle situation there is old start script installed already

# If old start script is a soft link, we just remove it
[ -h "$mystart" ] && rm -f "$mystart"

# If old start script is a file, we rename it
[ -f "$mystart" ] && mv -f "$mystart" "$mystart.old.$$"

# ----------------------------------------------------------------------

# We create a copy of an unmodified start script,
# as a reference for the one maybe modifying it

cp -f "$mystart1.in" "$mystart.in" || exit 1

# We rewrite some scripts

for script in "$mystart" "$mystart1" "$myinstdb" ; do
  script_in="$script.in"
  sed -e "s,@basedir@,$mybasedir,g" \
      -e "s,@datadir@,$mydatadir,g" "$script_in" > "$script"
  chmod u+x $script
done

rm -f "$mystart.in"

exit 0


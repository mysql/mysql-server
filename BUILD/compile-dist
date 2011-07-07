#!/bin/sh

# Copyright (c) 2004-2008 MySQL AB, 2008-2010 Sun Microsystems, Inc.
# Use is subject to license terms.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#
# This script's purpose is to update the automake/autoconf helper scripts and
# to run a plain "configure" without any special compile flags. Only features
# that affect the content of the source distribution are enabled. The resulting
# tree can then be picked up by "make dist" to create the "pristine source
# package" that is used as the basis for all other binary builds.
#
test -f Makefile && make maintainer-clean

path=`dirname $0`
. $path/autorun.sh

gmake=
for x in gmake gnumake make; do
  if $x --version 2>/dev/null | grep GNU > /dev/null; then
    gmake=$x
    break;
  fi
done

if [ -z "$gmake" ]; then
  # Our build may not depend on GNU make, but I wouldn't count on it
  echo "Please install GNU make, and ensure it is in your path as gnumake, gmake, or make" >&2
  exit 2
fi

# Default to gcc for CC and CXX
if test -z "$CXX" ; then
  export CXX
  CXX=gcc
  # Set some required compile options
  if test -z "$CXXFLAGS" ; then
    export CXXFLAGS
    CXXFLAGS="-felide-constructors -fno-exceptions -fno-rtti"
  fi
fi

if test -z "$CC" ; then
  export CC
  CC=gcc
fi


# Use ccache, if available
if ccache -V > /dev/null 2>&1
then
  if echo "$CC" | grep -v ccache > /dev/null
  then
    export CC
    CC="ccache $CC"
  fi
  if echo "$CXX" | grep -v ccache > /dev/null
  then
    export CXX
    CXX="ccache $CXX"
  fi
fi

# Make sure to enable all features that affect "make dist"
# Remember that configure restricts the man pages to the configured features !
./configure \
  --with-embedded-server \
  --with-perfschema \
  --with-ndbcluster
$gmake


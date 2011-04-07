#!/bin/sh

# Copyright (C) 2001, 2005 MySQL AB
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

make -k clean || true
/bin/rm -f */.deps/*.P config.cache
 
path=`dirname $0`
. "$path/autorun.sh"
 
CFLAGS="-g -Wimplicit -Wreturn-type -Wswitch -Wtrigraphs -Wcomment -W -Wchar-subscripts -Wformat -Wparentheses -Wsign-compare -Wwrite-strings -Wunused  -O3 -fno-omit-frame-pointer" CXX=gcc CXXFLAGS="-g -Wimplicit -Wreturn-type -Wswitch -Wtrigraphs -Wcomment -W -Wchar-subscripts -Wformat -Wparentheses -Wsign-compare -Wwrite-strings -Woverloaded-virtual -Wsign-promo -Wreorder -Wctor-dtor-privacy -Wnon-virtual-dtor -felide-constructors -fno-exceptions -fno-rtti  -O3 -fno-omit-frame-pointer" ./configure --prefix=/usr/local/mysql --enable-assembler --with-extra-charsets=complex --enable-thread-safe-client --with-debug

make -j 4

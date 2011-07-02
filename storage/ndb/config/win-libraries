#!/bin/sh

# Copyright (C) 2004 MySQL AB
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

dst=$1
shift

type=$1
shift

add_lib(){
    echo `dirname $2`/$1/`basename $2 | sed "s/\.[l]*a/$3.lib/g"`
}

out_rel=
out_deb=
out_tls_rel=
out_tls_deb=
for i in $*
do
# mysql VC++ project files have for some unknown reason
# choosen NOT to put libdbug.lib in $(topdir)./dbug but rather in $(topdir)
# the same goes for mysys and strings
  lib=$i
  case $i in
  *libdbug.a | *libmysys.a | *libmystrings.a) 
  lib=`echo $i | sed s'!dbug\/lib!!' | sed 's!mysys\/lib!!' | sed 's!strings\/libmy!!'`
  echo "Changing from $i to $lib"
  ;;
  esac
 
  if [ `echo $i | grep -c gcc` -eq 0 ]
      then
      out_rel="${out_rel} `add_lib lib_release $lib`"
      out_deb="${out_deb} `add_lib lib_debug $lib`"
      out_tls_rel="${out_tls_rel} `add_lib lib_release $lib _tls`"
      out_tls_deb="${out_tls_deb} `add_lib lib_debug $lib _tls`"
  fi
done

fix(){
    echo "# ADD BASE ${type}32 $*\n# ADD ${type}32 $*\n"
}

if [ "$out_rel" ]
then
    out_rel=`fix $out_rel`
    out_deb=`fix $out_deb`
    out_tls_rel=`fix $out_tls_rel`
    out_tls_deb=`fix $out_tls_deb`
fi

sed -e "s!@release_libs@!$out_rel!g" \
    -e "s!@debug_libs@!$out_deb!g" \
    -e "s!@tls_release_libs@!$out_tls_rel!g" \
    -e "s!@tls_debug_libs@!$out_tls_deb!g" \
 $dst > !tmp!$dst.$$
mv !tmp!$dst.$$ $dst

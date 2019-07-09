#!/bin/sh
#
# Copyright (c) 2007, 2016, Oracle and/or its affiliates. All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA


TMPF=t.$$

make -f Makefile.query

echo '#include "univ.i"' > $TMPF

# This is to avoid compiler warning about unused parameters.
# FIXME: gcc extension "MY_ATTRIBUTE" causing compilation errors on windows
# platform. Quote them out for now.
sed -e '
s/^\(static.*void.*yy_fatal_error.*msg.*,\)\(.*yyscanner\)/\1 \2 MY_ATTRIBUTE((unused))/;
s/^\(static.*void.*yy_flex_strncpy.*n.*,\)\(.*yyscanner\)/\1 \2 MY_ATTRIBUTE((unused))/;
s/^\(static.*int.*yy_flex_strlen.*s.*,\)\(.*yyscanner\)/\1 \2 MY_ATTRIBUTE((unused))/;
s/^\(\(static\|void\).*fts0[bt]alloc.*,\)\(.*yyscanner\)/\1 \3 MY_ATTRIBUTE((unused))/;
s/^\(\(static\|void\).*fts0[bt]realloc.*,\)\(.*yyscanner\)/\1 \3 MY_ATTRIBUTE((unused))/;
s/^\(\(static\|void\).*fts0[bt]free.*,\)\(.*yyscanner\)/\1 \3 MY_ATTRIBUTE((unused))/;
' < fts0blex.cc >> $TMPF

mv $TMPF fts0blex.cc

echo '#include "univ.i"' > $TMPF

sed -e '
s/^\(static.*void.*yy_fatal_error.*msg.*,\)\(.*yyscanner\)/\1 \2 MY_ATTRIBUTE((unused))/;
s/^\(static.*void.*yy_flex_strncpy.*n.*,\)\(.*yyscanner\)/\1 \2 MY_ATTRIBUTE((unused))/;
s/^\(static.*int.*yy_flex_strlen.*s.*,\)\(.*yyscanner\)/\1 \2 MY_ATTRIBUTE((unused))/;
s/^\(\(static\|void\).*fts0[bt]alloc.*,\)\(.*yyscanner\)/\1 \3 MY_ATTRIBUTE((unused))/;
s/^\(\(static\|void\).*fts0[bt]realloc.*,\)\(.*yyscanner\)/\1 \3 MY_ATTRIBUTE((unused))/;
s/^\(\(static\|void\).*fts0[bt]free.*,\)\(.*yyscanner\)/\1 \3 MY_ATTRIBUTE((unused))/;
' < fts0tlex.cc >> $TMPF

mv $TMPF fts0tlex.cc

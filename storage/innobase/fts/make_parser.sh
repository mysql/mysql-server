#!/bin/sh
#
# Copyright (c) 2007, 2011, Oracle and/or its affiliates. All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA


TMPF=t.$$

make -f Makefile.query

echo '#include "univ.i"' > $TMPF

# This is to avoid compiler warning about unused parameters.
# FIXME: gcc extension "__attribute__" causing compilation errors on windows
# platform. Quote them out for now.
sed -e '
s/^\(static.*void.*yy_fatal_error.*msg.*,\)\(.*yyscanner\)/\1 \2 __attribute__((unused))/;
s/^\(static.*void.*yy_flex_strncpy.*n.*,\)\(.*yyscanner\)/\1 \2 __attribute__((unused))/;
s/^\(static.*int.*yy_flex_strlen.*s.*,\)\(.*yyscanner\)/\1 \2 __attribute__((unused))/;
s/^\(\(static\|void\).*fts0[bt]alloc.*,\)\(.*yyscanner\)/\1 \3 __attribute__((unused))/;
s/^\(\(static\|void\).*fts0[bt]realloc.*,\)\(.*yyscanner\)/\1 \3 __attribute__((unused))/;
s/^\(\(static\|void\).*fts0[bt]free.*,\)\(.*yyscanner\)/\1 \3 __attribute__((unused))/;
' < fts0blex.cc >> $TMPF

mv $TMPF fts0blex.cc

echo '#include "univ.i"' > $TMPF

sed -e '
s/^\(static.*void.*yy_fatal_error.*msg.*,\)\(.*yyscanner\)/\1 \2 __attribute__((unused))/;
s/^\(static.*void.*yy_flex_strncpy.*n.*,\)\(.*yyscanner\)/\1 \2 __attribute__((unused))/;
s/^\(static.*int.*yy_flex_strlen.*s.*,\)\(.*yyscanner\)/\1 \2 __attribute__((unused))/;
s/^\(\(static\|void\).*fts0[bt]alloc.*,\)\(.*yyscanner\)/\1 \3 __attribute__((unused))/;
s/^\(\(static\|void\).*fts0[bt]realloc.*,\)\(.*yyscanner\)/\1 \3 __attribute__((unused))/;
s/^\(\(static\|void\).*fts0[bt]free.*,\)\(.*yyscanner\)/\1 \3 __attribute__((unused))/;
' < fts0tlex.cc >> $TMPF

mv $TMPF fts0tlex.cc

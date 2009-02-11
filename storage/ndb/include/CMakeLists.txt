# Copyright (C) 2007 MySQL AB
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

INCLUDE(CheckTypeSize)
CHECK_TYPE_SIZE(char NDB_SIZEOF_CHAR)
CHECK_TYPE_SIZE(short NDB_SIZEOF_SHORT)
CHECK_TYPE_SIZE(int NDB_SIZEOF_INT)
CHECK_TYPE_SIZE(long NDB_SIZEOF_LONG)
CHECK_TYPE_SIZE("char*" NDB_SIZEOF_CHARP)
CHECK_TYPE_SIZE("long long" NDB_SIZEOF_LONG_LONG)


CONFIGURE_FILE(ndb_global.h.in
               ${CMAKE_CURRENT_SOURCE_DIR}/ndb_global.h
               @ONLY)
CONFIGURE_FILE(ndb_types.h.in
               ${CMAKE_CURRENT_SOURCE_DIR}/ndb_types.h
               @ONLY)
CONFIGURE_FILE(ndb_version.h.in
               ${CMAKE_CURRENT_SOURCE_DIR}/ndb_version.h
               @ONLY)

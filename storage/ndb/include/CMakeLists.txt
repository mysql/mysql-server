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

# Check the size of our types and configure ndb_types.h
INCLUDE(CheckTypeSize)
CHECK_TYPE_SIZE(char NDB_SIZEOF_CHAR)
CHECK_TYPE_SIZE(short NDB_SIZEOF_SHORT)
CHECK_TYPE_SIZE(int NDB_SIZEOF_INT)
CHECK_TYPE_SIZE(long NDB_SIZEOF_LONG)
CHECK_TYPE_SIZE("char*" NDB_SIZEOF_CHARP)
CHECK_TYPE_SIZE("long long" NDB_SIZEOF_LONG_LONG)
CONFIGURE_FILE(ndb_types.h.in
               ${CMAKE_CURRENT_SOURCE_DIR}/ndb_types.h
               @ONLY)

#
# Read ndb_configure.m4 and extract the NDB_VERSION_XX=YY variables
#
FILE(READ "${CMAKE_SOURCE_DIR}/storage/ndb/ndb_configure.m4" CONFIGURE_IN)

# NDB_VERSION_MAJOR
STRING(REGEX REPLACE ".*NDB_VERSION_MAJOR=([0-9]+).*"
        "\\1" PARSE_NDB_VERSION_MAJOR "${CONFIGURE_IN}")
SET(NDB_VERSION_MAJOR "${PARSE_NDB_VERSION_MAJOR}" CACHE STRING "NDB Major Version" FORCE)

# NDB_VERSION_MINOR
STRING(REGEX REPLACE ".*NDB_VERSION_MINOR=([0-9]+).*"
        "\\1" PARSE_NDB_VERSION_MINOR "${CONFIGURE_IN}")
SET(NDB_VERSION_MINOR "${PARSE_NDB_VERSION_MINOR}" CACHE STRING "NDB Minor Version" FORCE)

# NDB_VERSION_BUILD
STRING(REGEX REPLACE ".*NDB_VERSION_BUILD=([0-9]+).*"
        "\\1" PARSE_NDB_VERSION_BUILD "${CONFIGURE_IN}")
SET(NDB_VERSION_BUILD "${PARSE_NDB_VERSION_BUILD}" CACHE STRING "NDB Build Version" FORCE)

# NDB_VERSION_STATUS               
STRING(REGEX REPLACE ".*NDB_VERSION_STATUS=\"([^\"]+)\".*"
        "\\1" PARSE_NDB_VERSION_STATUS "${CONFIGURE_IN}")
SET(NDB_VERSION_STATUS "${NDB_VERSION_STATUS}" CACHE STRING "NDB Status Version" FORCE)

# Create ndb_version.h
CONFIGURE_FILE(ndb_version.h.in
               ${CMAKE_CURRENT_SOURCE_DIR}/ndb_version.h
               @ONLY)


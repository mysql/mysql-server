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
               
# Split VERSION(mysql-5.1.37-xxx) into NDB_MYSQL_VERSION_XXX with
# one variable for minor, major and build

# NDB_MYSQL_VERSION_MAJOR
STRING(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)-?(.*)"
        "\\1" PARSE_VERSION_MAJOR "${VERSION}")
SET(NDB_MYSQL_VERSION_MAJOR "${PARSE_VERSION_MAJOR}" CACHE STRING "NDB MySQL Major Version" FORCE)

# NDB_MYSQL_VERSION_MINOR
STRING(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)-?(.*)"
        "\\2" PARSE_VERSION_MINOR "${VERSION}")
SET(NDB_MYSQL_VERSION_MINOR "${PARSE_VERSION_MINOR}" CACHE STRING "NDB MySQL Minor Version" FORCE)

# NDB_MYSQL_VERSION_BUILD
STRING(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)-?(.*)"
        "\\3" PARSE_VERSION_BUILD "${VERSION}")
SET(NDB_MYSQL_VERSION_BUILD "${PARSE_VERSION_BUILD}" CACHE STRING "NDB MySQL Build Version" FORCE)

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
SET(NDB_VERSION_STATUS "${NDB_VERSION_STATUS}" CACHE STRING "NDB Staus Version" FORCE)

# Create ndb_version.h
CONFIGURE_FILE(ndb_version.h.in
               ${CMAKE_CURRENT_SOURCE_DIR}/ndb_version.h
               @ONLY)


# Copyright (C) 2006 MySQL AB
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

SET(ARCHIVE_PLUGIN_STATIC  "archive")
SET(ARCHIVE_PLUGIN_DYNAMIC "ha_archive")

SET(ARCHIVE_SOURCES  azio.c ha_archive.cc ha_archive.h)
MYSQL_STORAGE_ENGINE(ARCHIVE)
IF(NOT WITH_ARCHIVE_STORAGE_ENGINE AND NOT WITHOUT_ARCHIVE_STORAGE_ENGINE 
   AND NOT WITH_ZLIB STREQUAL "bundled")
  TARGET_LINK_LIBRARIES(archive ${ZLIB_LIBRARY})
ENDIF()



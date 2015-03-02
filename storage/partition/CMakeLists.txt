# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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

SET(PARTITION_SOURCES ha_partition.cc)
IF(WITH_PARTITION_STORAGE_ENGINE AND NOT WITHOUT_PARTITION_STORAGE_ENGINE)
  MYSQL_ADD_PLUGIN(partition ${PARTITION_SOURCES} STORAGE_ENGINE DEFAULT
                   STATIC_ONLY RECOMPILE_FOR_EMBEDDED)
ELSEIF(NOT WITHOUT_PARTITION_STORAGE_ENGINE)
  MYSQL_ADD_PLUGIN(partition ${PARTITION_SOURCES} STORAGE_ENGINE MODULE_ONLY)
ENDIF()

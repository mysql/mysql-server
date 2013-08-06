# Copyright (c) 2006, 2013, Oracle and/or its affiliates. All rights reserved.
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

SET(FEDERATED_PLUGIN_STATIC  "federated")
SET(FEDERATED_PLUGIN_DYNAMIC "ha_federated")
SET(FEDERATED_SOURCES  ha_federated.cc)
IF(NOT WITH_FEDERATED AND NOT WITH_FEDERATED_STORAGE_ENGINE)
 # Bug#45488- federated uses symbols that are not used anywhere in
 # mysqld and are optimized away by the linker.
 SET(FEDERATED_SOURCES ${FEDERATED_SOURCES} ${CMAKE_SOURCE_DIR}/mysys/string.c)
ENDIF()
MYSQL_ADD_PLUGIN(federated ${FEDERATED_SOURCES} STORAGE_ENGINE
RECOMPILE_FOR_EMBEDDED)

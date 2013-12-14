# Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.
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
IF(WITH_INNODB_MEMCACHED AND UNIX)
  ADD_SUBDIRECTORY(daemon_memcached)
  ADD_SUBDIRECTORY(innodb_memcache)
ENDIF()

INSTALL_SCRIPT("${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/innodb_memcache/scripts/innodb_memcached_config.sql" DESTINATION ${INSTALL_MYSQLSHAREDIR} COMPONENT Server)

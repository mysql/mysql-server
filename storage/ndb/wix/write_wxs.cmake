# Copyright (c) 2013, 2024, Oracle and/or its affiliates.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
#
# This script is never run by itself. It is appended to 
# to a modified copy of packaging/WiX/create_msi.cmake to write a wxs suitable for MySQL Cluster.

# Make sure configuration is Release or RelWithDebInfo. Other configurations create different 
# installation layouts which the wxs-creation scripts don't handle. 
IF(NOT CMAKE_INSTALL_CONFIG_NAME MATCHES "Rel")
	MESSAGE(FATAL_ERROR "Configuration ${CMAKE_INSTALL_CONFIG_NAME} cannot be used to build msi")
ENDIF()

# "Unset" variables that are
# 1) referenced in mysql_server.wxs.in, and 
# 2) have been assigned a value that either unsuitable for MySQL Cluster 

# This includes _VERSION variables (hard-coded to the server version), or WIX_ variables that 
# contain incorrect xml when Cluster components are added to the install tree.
FOREACH(v MAJOR_VERSION MINOR_VERSION PATCH_VERSION CPACK_WIX_DIRECTORIES CPACK_WIX_COMPONENTS CPACK_WIX_COMPONENT_GROUPS)
	SET(${v} "@${v}@")
ENDFOREACH()

# Write an intermediate wxs.in file specific to MySQL Cluster, 
# which can be used by ndb_create_wxs.cmake to create a correct wxs for MySQL Cluster.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/mysql_server.wxs.in"
	"${CMAKE_CURRENT_BINARY_DIR}/${WXS_BASENAME}.wxs.in" @ONLY)

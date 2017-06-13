# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

REMOVE_DEFINITIONS(-DMYSQL_SERVER)

MYSQL_ADD_COMPONENT(test_pfs_resource_group
  test_pfs_resource_group.cc
  TEST MODULE)

TARGET_LINK_LIBRARIES(component_test_pfs_resource_group mysqlservices mysys)

MYSQL_ADD_COMPONENT(test_pfs_notification
  test_pfs_notification.cc
  TEST MODULE)

TARGET_LINK_LIBRARIES(component_test_pfs_notification mysqlservices)


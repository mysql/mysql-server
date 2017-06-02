# Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.
#
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
# along with this program; if not, write to the Free Software Foundation,
# 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}
                    ${CMAKE_SOURCE_DIR}/include)

ADD_DEFINITIONS(-DMYSQL_SERVER)

SET(PFS_EXAMPLE_PLUGIN_EMPLOYEE_SOURCE
    pfs_example_plugin_employee.cc
    pfs_example_employee_name.cc
    pfs_example_employee_salary.cc
    pfs_example_machine.cc
    pfs_example_machines_by_emp_by_mtype.cc)

MYSQL_ADD_PLUGIN(pfs_example_plugin_employee ${PFS_EXAMPLE_PLUGIN_EMPLOYEE_SOURCE} TEST_ONLY MODULE_ONLY)

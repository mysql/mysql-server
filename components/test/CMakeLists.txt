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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/sql)

REMOVE_DEFINITIONS(-DMYSQL_SERVER)

MYSQL_ADD_COMPONENT(test_udf_registration
  test_udf_registration.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(udf_reg_3_func
  udf_reg_3_func.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(udf_reg_only_3_func
  udf_reg_only_3_func.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(udf_unreg_3_func
  udf_unreg_3_func.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(udf_reg_int_func
  udf_reg_int_func.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(udf_unreg_int_func
  udf_unreg_int_func.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(udf_reg_int_same_func
  udf_reg_int_same_func.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(udf_reg_real_func
  udf_reg_real_func.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(udf_unreg_real_func
  udf_unreg_real_func.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(udf_reg_avg_func
  udf_reg_avg_func.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(test_sys_var_service
  test_sys_var_service.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(test_sys_var_service_same
  test_sys_var_service_same.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(test_sys_var_service_int
  test_sys_var_service_int.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(test_sys_var_service_str
  test_sys_var_service_str.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(test_status_var_service
  test_status_var_service.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(test_status_var_service_int
  test_status_var_service_int.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(test_status_var_service_str
  test_status_var_service_str.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(test_status_var_service_reg_only
  test_status_var_service_reg_only.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(test_status_var_service_unreg_only
  test_status_var_service_unreg_only.cc
  TEST MODULE)

MYSQL_ADD_COMPONENT(test_system_variable_source
  test_system_variable_source.cc
  TEST MODULE)


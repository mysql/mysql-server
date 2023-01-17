# Copyright (c) 2017, 2023, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# We are not interesting in profiling tests.
DISABLE_MISSING_PROFILE_WARNING()
MSVC_CPPCHECK_DISABLE()

MYSQL_ADD_COMPONENT(test_udf_registration
  test_udf_registration.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(udf_reg_3_func
  udf_reg_3_func.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(udf_reg_only_3_func
  udf_reg_only_3_func.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(udf_unreg_3_func
  udf_unreg_3_func.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(udf_reg_int_func
  udf_reg_int_func.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(udf_unreg_int_func
  udf_unreg_int_func.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(udf_reg_int_same_func
  udf_reg_int_same_func.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(udf_reg_real_func
  udf_reg_real_func.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(udf_unreg_real_func
  udf_unreg_real_func.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(udf_reg_avg_func
  udf_reg_avg_func.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_sys_var_service
  test_sys_var_service.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_sys_var_service_same
  test_sys_var_service_same.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_sys_var_service_int
  test_sys_var_service_int.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_sys_var_service_str
  test_sys_var_service_str.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_status_var_service
  test_status_var_service.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_status_var_service_int
  test_status_var_service_int.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_status_var_service_str
  test_status_var_service_str.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_status_var_service_reg_only
  test_status_var_service_reg_only.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_status_var_service_unreg_only
  test_status_var_service_unreg_only.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_system_variable_source
  test_system_variable_source.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_host_application_signal
  test_host_application_signal.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_audit_api_message
  test_audit_api_message.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_mysql_runtime_error
  test_mysql_runtime_error.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_mysql_current_thread_reader
  test_mysql_current_thread_reader.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_component_deinit
  test_component_deinit.cc
  MODULE_ONLY
  TEST_ONLY
  LINK_LIBRARIES mysys
  )
MYSQL_ADD_COMPONENT(test_mysql_system_variable_set
  test_mysql_system_variable_set.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_sensitive_system_variables
  test_sensitive_system_variables.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_mysql_command_services
  test_mysql_command_services.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_status_var_reader
  test_status_var_reader.cc
  MODULE_ONLY
  TEST_ONLY
  )
MYSQL_ADD_COMPONENT(test_mysql_thd_store_service
  test_mysql_thd_store_service.cc
  MODULE_ONLY
  TEST_ONLY
  )

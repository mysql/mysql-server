# Copyright (c) 2020, 2024, Oracle and/or its affiliates.
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

SET(LIBMINCHASSIS_SOURCES
  component_common.cc
  dynamic_loader.cc
  dynamic_loader_scheme_file.cc
  mc_psi_system_service.cc
  mc_rwlock_service.cc
  minimal_chassis.cc
  minimal_chassis_runtime_error_imp.cc
  my_metadata.cc
  my_ref_counted.cc
  mysql_component.cc
  mysql_service_implementation.cc
  registry.cc
  registry_no_lock.cc
  rwlock_scoped_lock.cc
)

ADD_LIBRARY(minchassis STATIC ${LIBMINCHASSIS_SOURCES})
ADD_DEPENDENCIES(minchassis GenError)
TARGET_LINK_LIBRARIES(minchassis ${CMAKE_DL_LIBS})

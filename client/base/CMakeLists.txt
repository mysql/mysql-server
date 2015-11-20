# Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.
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

ADD_DEFINITIONS(${SSL_DEFINES})

ADD_CONVENIENCE_LIBRARY(client_base
  i_option.cc
  bool_option.cc
  char_array_option.cc
  disabled_option.cc
  password_option.cc
  simple_option.cc
  string_option.cc

  abstract_options_provider.cc
  composite_options_provider.cc
  debug_options.cc
  help_options.cc
  mysql_connection_options.cc
  ssl_options.cc

  abstract_program.cc
  abstract_connection_program.cc

  message_data.cc
  mysql_query_runner.cc
  show_variable_query_extractor.cc

  mutex.cc

  ../get_password.c

  ../../sql-common/sql_string.cc
)

ADD_COMPILE_FLAGS(
  mutex.cc
  mysql_query_runner.cc
  show_variable_query_extractor.cc
  COMPILE_FLAGS -I${BOOST_PATCHES_DIR} -I${BOOST_INCLUDE_DIR}
)

TARGET_LINK_LIBRARIES(client_base mysqlclient)

# Do not build library unless it is needed by some other target.
SET_PROPERTY(TARGET client_base PROPERTY EXCLUDE_FROM_ALL TRUE)

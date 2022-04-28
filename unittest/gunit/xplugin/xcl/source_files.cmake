# Copyright (c) 2020, 2022, Oracle and/or its affiliates.
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


SET(XCL_TEST_SRC
  auth_chaining_t.cc
  connection_general_t.cc
  mock/mock.cc
  cyclic_buffer_t.cc
  protocol_auth_t.cc
  protocol_execute_t.cc
  protocol_global_error_t.cc
  protocol_notices_t.cc
  protocol_send_recv_t.cc
  query_t.cc
  session_capability_t.cc
  session_connect_t.cc
  session_execute_t.cc
  session_general_t.cc
  session_negotiation_t.cc
  session_options_t.cc
  sha256_scramble_t.cc
  ssl_config_t.cc
  xpriority_list_t.cc
)


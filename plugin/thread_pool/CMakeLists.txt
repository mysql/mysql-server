# Copyright (c) 2011, 2026, Oracle and/or its affiliates.
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


CHECK_FUNCTION_EXISTS(epoll_wait HAVE_EPOLL_WAIT)

IF(HAVE_EPOLL_WAIT)
  SET(TP_METHOD src/epoll.cc)
ELSE()
  SET(TP_METHOD src/poll.cc)
ENDIF()

MYSQL_ADD_PLUGIN(thread_pool
  src/thread_pool.cc
  src/thread_pool_plugin.cc
  src/thread_pool_tables.cc
  src/table_thread_state.cc
  src/table_thread_group_state.cc
  src/table_thread_group_stats.cc
  src/table_connections.cc
  src/option_usage.cc
  ${TP_METHOD} MODULE_ONLY
  LINK_LIBRARIES extra::rapidjson library_mysys)

ADD_DEFINITIONS(-DMYSQL_SERVER)
ADD_DEFINITIONS(-DLOG_COMPONENT_TAG="thread_pool")


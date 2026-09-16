# Copyright (c) 2026, Oracle and/or its affiliates.
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

SET(TARGET_HEADERS
  cache_line_size.h
  condition_variable.h
  key_stub.h
  locking_queue.h
  mutex.h
  mutex_wrapper.h
  padded_slot.h
  spin_lock_mutex.h
  stage.h
  sync_bounded_queue.h
  sync_bounded_queue_impl.hpp
  thread.h
)

SET(TARGET_HEADERS_SERVER
  ${TARGET_HEADERS}
  condition_variable_wrapper.h
  condition_variable_wrapper_impl.hpp
  mutex_srv.h
  mutex_wrapper.h
  stage_srv.h
  thread_srv.h
  thread_srv_impl.hpp
)

SET(TARGET_HEADERS_STANDALONE
  ${TARGET_HEADERS}
  condition_variable_stl.h
  mutex_stl.h
  stage_stl.h
  thread_stl.h
)

SET(TARGET_SRCS
  spin_lock_mutex.cpp
)

SET(TARGET_SRCS_STANDALONE
  ${TARGET_SRCS}
)

SET(TARGET_SRCS_SERVER
  ${TARGET_SRCS}
  mutex_wrapper.cpp
  condition_variable_wrapper.cpp
)

LIBS_MYSQL_CREATE_LIBRARY(mysql_concurrency_server
  TARGET_SRCS ${TARGET_SRCS_SERVER}
  TARGET_HEADERS ${TARGET_HEADERS_SERVER}
  LINK_LIBRARIES ${LINK_LIBRARIES} mysys
  DEPENDENCIES GenError
)

LIBS_MYSQL_CREATE_LIBRARY(mysql_concurrency
  TARGET_SRCS ${TARGET_SRCS_STANDALONE}
  TARGET_HEADERS ${TARGET_HEADERS_STANDALONE}
  LINK_LIBRARIES ${LINK_LIBRARIES}
  COMPILE_DEFINITIONS STANDALONE_LIBS_MYSQL
  DEPENDENCIES GenError
)

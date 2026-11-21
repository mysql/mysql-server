# Copyright (c) 2021, 2026, Oracle and/or its affiliates.
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

ADD_DEFINITIONS(-DSTANDALONE_BINLOG)

MY_CHECK_CXX_COMPILER_WARNING("-Wno-unused-local-typedefs" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
ENDIF()

INCLUDE_DIRECTORIES(SYSTEM ${GMOCK_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(SYSTEM
  ${CMAKE_SOURCE_DIR}/extra/unordered_dense/unordered_dense-4.4.0/include)

# Add tests
#
# The imr_* tests are not in this list; they are defined below.
SET(TESTS
  cstreams_reader_state)

SET(TESTS_LINK_LIBRARIES
  changestreams_standalone_static
  mysql_binlog_event_standalone
  mysqlclient
  gunit_small)

IF (WITH_CS_PROTOBUF)
  LIST(APPEND TESTS
    cstreams_reader_state_codec_pb)

  LIST(APPEND TESTS_LINK_LIBRARIES
    changestreams_pb_static
    ext::libprotobuf-lite)
ENDIF()

FOREACH(test ${TESTS})
  MYSQL_ADD_EXECUTABLE(${test}-t ${test}-t.cc
    ADD_TEST ${test}
    LINK_LIBRARIES ${TESTS_LINK_LIBRARIES}
    COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}")
ENDFOREACH()

# The imr_* tests link gunit_large + server_unittest_library, because the
# queue builds the byte source, which needs Log_event and other server symbols.
# The in-memory sources are listed in sql/CMakeLists.txt, so they are already in
# that library and these targets need no EXTRA_SRC. The standalone binlog libs
# are not linked: they are built with -DSTANDALONE_BINLOG and clash with the
# server binlog objects in the library. For the same reason the imr_* targets
# are built without -DSTANDALONE_BINLOG, which is removed here: it would select
# the non-mysys inline binlog helpers, which do not match the library.
REMOVE_DEFINITIONS(-DSTANDALONE_BINLOG)

FOREACH(imr_core_test
    imr_queue_admission
    imr_trx_payload
    imr_transaction_envelope
    imr_queue_fifo
    imr_queue_lifecycle
    imr_sweep_and_dispatch
    imr_spill_writer
    imr_spill_encryption
    imr_event_set_fetchable_spill
    imr_reconcile_property
    imr_attachment_independence
    imr_reset_property
    imr_spill_purge_property
    imr_memory_resource)
  MYSQL_ADD_EXECUTABLE(${imr_core_test}-t ${imr_core_test}-t.cc
    ADD_TEST ${imr_core_test}
    LINK_LIBRARIES gunit_large server_unittest_library
    COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}")
ENDFOREACH()

MYSQL_ADD_EXECUTABLE(imr_event_set_fetchable_memory-t
  imr_event_set_fetchable_memory-t.cc
  ADD_TEST imr_event_set_fetchable_memory
  LINK_LIBRARIES gunit_large server_unittest_library
  COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}")

# Queued_transaction_reader::read() builds a real Job_applier.
MYSQL_ADD_EXECUTABLE(imr_queued_transaction_reader-t
  imr_queued_transaction_reader-t.cc
  ADD_TEST imr_queued_transaction_reader
  LINK_LIBRARIES gunit_large server_unittest_library
  COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}")

# End-to-end memory path: queue, byte source and reader together.
MYSQL_ADD_EXECUTABLE(imr_integration-t
  imr_integration-t.cc
  ADD_TEST imr_integration
  LINK_LIBRARIES gunit_large server_unittest_library
  COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}")

# The receiver write helpers (queued_transaction_writer.cc) on a real queue.
MYSQL_ADD_EXECUTABLE(imr_queued_transaction_writer-t
  imr_queued_transaction_writer-t.cc
  ADD_TEST imr_queued_transaction_writer
  LINK_LIBRARIES gunit_large server_unittest_library
  COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}")


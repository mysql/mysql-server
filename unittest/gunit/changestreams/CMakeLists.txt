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
# NOTE: none of the imr_* tests are in this lightweight list. As of the
# destination-creating enqueue() extension, Trx_envelope_queue::enqueue() itself
# constructs the memory-path byte source (Event_set_fetchable_memory), so the
# queue TU now transitively references the real server internals (Log_event and
# its decode/decompression istream, server globals). Any test that links the
# queue therefore needs those symbols, so ALL imr_* tests are built below
# against gunit_large + server_unittest_library — the conventional MySQL
# mechanism for a gunit test that exercises server internals (as used by the
# sibling binlogevents legacy_gtid_set-t and the main SERVER_TESTS). The tests
# in this list stay on the lightweight standalone-lib link set.
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

# The in-memory byte source test needs the REAL server internals: Log_event and
# its decode path, binlog::Decompressing_event_object_istream, and the server
# globals the Format_description_log_event vtable pulls in. Rather than compile
# sql/log_event.cc (+ the rpl_*/istream cluster) into an isolated harness and
# satisfy the residue with hand-written link-time stubs, link the conventional
# MySQL server unit-test library (gunit_large + server_unittest_library) — a
# test-only merge of sql_main/binlog/perfschema/... that already provides real
# Log_event/server symbols. This mirrors the sibling binlogevents
# legacy_gtid_set-t and the main SERVER_TESTS. All of the in-memory core sources
# (event_set_fetchable_memory.cpp, trx_envelope_queue.cpp, transaction_envelope.cpp,
# trx_payload.cpp, fetchable_transaction.cpp, managed_event.cpp) are registered
# in sql/CMakeLists.txt and are therefore already inside server_unittest_library,
# so this target needs no EXTRA_SRC — the linker pulls the real objects from the
# library. The standalone binlog libs (mysql_binlog_event_standalone /
# changestreams_standalone_static) are deliberately NOT linked here: they are
# built under -DSTANDALONE_BINLOG and clash (duplicate symbols / ODR) with the
# -DMYSQL_SERVER binlog objects in server_unittest_library.
#
# This target must ALSO be compiled without -DSTANDALONE_BINLOG. That macro is
# added directory-wide (ADD_DEFINITIONS above) and only flips HAVE_MYSYS off in
# the binlog-event wrapper/byteorder headers, selecting the standalone
# (non-mysys) inline helpers. server_unittest_library, however, is built with
# HAVE_MYSYS on (-DMYSQL_SERVER, no STANDALONE_BINLOG). If this TU kept
# STANDALONE_BINLOG, the Log_event objects it allocates/frees would cross that
# HAVE_MYSYS boundary via mismatched inline code (real Format_description_log_event
# built in the library, destroyed through the TU's standalone inlines), which
# crashes on teardown. Dropping STANDALONE_BINLOG here aligns this TU's shared
# inline/header code with the server library it links. The parent
# unittest/gunit/CMakeLists.txt already supplies -DMYSQL_SERVER, so removing
# STANDALONE_BINLOG leaves this TU in the same server build mode as the library.
# All targets defined after this REMOVE_DEFINITIONS are built without
# STANDALONE_BINLOG (server / HAVE_MYSYS mode), matching server_unittest_library.
REMOVE_DEFINITIONS(-DSTANDALONE_BINLOG)

# The in-memory core tests (admission, trx_payload, transaction_envelope, FIFO
# ordering) now link the real server library too: since enqueue() constructs the
# Event_set_fetchable_memory byte source, the Trx_envelope_queue TU these tests
# link references Log_event/decode/decompression-istream and server globals that
# only exist in server_unittest_library. All the in-memory sources they exercise
# are registered in sql/CMakeLists.txt and are therefore already inside
# server_unittest_library, so these targets need no EXTRA_SRC — the linker pulls
# the real objects from the library. Like the heavy targets below, they must NOT
# be compiled with -DSTANDALONE_BINLOG (removed directory-wide just above).
FOREACH(imr_core_test
    imr_queue_admission
    imr_trx_payload
    imr_transaction_envelope
    imr_queue_fifo
    imr_queue_lifecycle
    imr_sweep_and_dispatch
    imr_spill_writer
    imr_event_set_fetchable_spill)
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

# imr_queued_transaction_reader gets the SAME standalone treatment as the byte
# source above. Queued_transaction_reader::read() builds a real Job_applier
# (pulling in the apply/session/statistics stack) and references server globals
# (slave_trans_retries), so it needs the real server symbols from
# server_unittest_library rather than the standalone binlog libs. The reader's
# own source (queued_transaction_reader.cpp) plus the imr core sources are
# already registered in sql/CMakeLists.txt and therefore already inside
# server_unittest_library, so this target needs no EXTRA_SRC. Like the byte
# source, it must NOT be compiled with -DSTANDALONE_BINLOG (removed
# directory-wide above); this target is defined after that REMOVE_DEFINITIONS so
# it inherits the server (HAVE_MYSYS) build mode of the library it links.
MYSQL_ADD_EXECUTABLE(imr_queued_transaction_reader-t
  imr_queued_transaction_reader-t.cc
  ADD_TEST imr_queued_transaction_reader
  LINK_LIBRARIES gunit_large server_unittest_library
  COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}")

# imr_integration gets the SAME standalone treatment as the byte source and the
# reader above. The end-to-end harness wires all five in-memory-relaylog classes
# together and exercises the full memory path at once: the byte source's
# Log_event decode path, the Fetchable_transaction consumer surface, and
# Queued_transaction_reader::read() building a real Job_applier (pulling in the
# apply/session/statistics stack and server globals). It therefore needs the
# real server symbols from server_unittest_library rather than the standalone
# binlog libs. All of the in-memory sources it drives are already registered in
# sql/CMakeLists.txt and therefore already inside server_unittest_library, so
# this target needs no EXTRA_SRC. Like the byte source and the reader, it must
# NOT be compiled with -DSTANDALONE_BINLOG (removed directory-wide above); this
# target is defined after that REMOVE_DEFINITIONS so it inherits the server
# (HAVE_MYSYS) build mode of the library it links.
MYSQL_ADD_EXECUTABLE(imr_integration-t
  imr_integration-t.cc
  ADD_TEST imr_integration
  LINK_LIBRARIES gunit_large server_unittest_library
  COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}")

# TEMPORARY / SPIKE (tasks.md task 12) — do not ship.
# imr_queued_transaction_writer-t exercises the standalone minimal-state
# receiver-helper module (queued_transaction_writer.{h,cc}) against a REAL
# Trx_envelope_queue plus a local Streaming_event_sink pointer (no Master_info).
# It reuses the real Cached_event_payload decode path and the enqueue-created
# byte source, so it needs the server symbols from server_unittest_library
# exactly like the other imr_* heavy targets above. The module source is
# registered in sql/CMakeLists.txt and therefore already inside
# server_unittest_library, so this target needs no EXTRA_SRC. Defined after the
# REMOVE_DEFINITIONS above so it is built without -DSTANDALONE_BINLOG, matching
# the library.
MYSQL_ADD_EXECUTABLE(imr_queued_transaction_writer-t
  imr_queued_transaction_writer-t.cc
  ADD_TEST imr_queued_transaction_writer
  LINK_LIBRARIES gunit_large server_unittest_library
  COMPILE_DEFINITIONS "${DISABLE_PSI_DEFINITIONS}")


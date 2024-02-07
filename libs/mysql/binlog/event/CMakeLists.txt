# Copyright (c) 2023, 2024, Oracle and/or its affiliates.
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

SET(LINK_LIBRARIES
  ext::zstd
  ext::zlib
  mysql_gtid
  mysql_serialization
)

SET(TARGET_HEADERS
  codecs/base.h
  codecs/binary.h
  codecs/factory.h
  compression/iterator.h
  export/binary_log_funcs.h
  binary_log.h
  binlog_event.h
  byteorder.h
  control_events.h
  debug_vars.h
  event_reader_macros.h
  event_reader.h
  load_data_events.h
  rows_event.h
  statement_events.h
  table_id.h
  trx_boundary_parser.h
  wrapper_functions.h
  compression/base.h
  compression/compressor.h
  compression/decompressor.h
  compression/factory.h
  compression/none_comp.h
  compression/none_dec.h
  compression/zstd_comp.h
  compression/zstd_dec.h
)

SET(TARGET_SRCS
  codecs/binary.cpp
  codecs/factory.cpp
  binary_log_funcs.cpp
  binlog_event.cpp
  control_events.cpp
  event_reader.cpp
  load_data_events.cpp
  rows_event.cpp
  statement_events.cpp
  trx_boundary_parser.cpp
  compression/base.cpp
  compression/compressor.cpp
  compression/decompress_status.cpp
  compression/decompressor.cpp
  compression/factory.cpp
  compression/none_comp.cpp
  compression/none_dec.cpp
  compression/payload_event_buffer_istream.cpp
  compression/zstd_comp.cpp
  compression/zstd_dec.cpp
  compression/buffer/grow_calculator.cpp
  compression/buffer/grow_constraint.cpp
  compression/buffer/grow_status.cpp
)

ADD_WSHADOW_WARNING()
DISABLE_MISSING_PROFILE_WARNING()

LIBS_MYSQL_CREATE_LIBRARY(mysql_binlog_event
  TARGET_SRCS ${TARGET_SRCS}
  TARGET_HEADERS ${TARGET_HEADERS}
  LINK_LIBRARIES ${LINK_LIBRARIES} mysys
  COMPILE_DEFINITIONS BINLOG_EVENT_COMPRESSION_USE_ZSTD_${WITH_ZSTD}
  DEPENDENCIES GenError
)

LIBS_MYSQL_CREATE_LIBRARY(mysql_binlog_event_standalone
  TARGET_SRCS ${TARGET_SRCS}
  TARGET_HEADERS ${TARGET_HEADERS}
  LINK_LIBRARIES ${LINK_LIBRARIES}
  COMPILE_DEFINITIONS STANDALONE_BINLOG BINLOG_EVENT_COMPRESSION_USE_ZSTD_${WITH_ZSTD}
)

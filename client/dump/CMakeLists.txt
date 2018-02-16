# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

# If we are building client-only (-DWITHOUT_SERVER=ON) with Solaris Studio,
# we skip building the mysqlpump tool. The server, and mysqlpump, will be built
# using gcc.

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/dump
)

IF (CMAKE_CXX_COMPILER_ID STREQUAL "SunPro")
  IF (WITHOUT_SERVER)
    MESSAGE(STATUS "Not building mysqlpump")
    RETURN()
  ENDIF()
ENDIF()

# Prevent Boost from including external precompiled Boost libraries, use
# threading (not implemented for older Solaris Studio compilers)
# and turn off unused functionality.
IF(USING_LOCAL_BOOST)
  ADD_DEFINITIONS(
    -DBOOST_ALL_NO_LIB
    -DBOOST_SYSTEM_NO_DEPRECATED)
ENDIF()

SET(BOOST_LIB_SOURCES
  ${BOOST_SOURCES_DIR}/libs/chrono/src/chrono.cpp
  ${BOOST_SOURCES_DIR}/libs/system/src/error_code.cpp
)

INCLUDE_DIRECTORIES(SYSTEM
  ${BOOST_PATCHES_DIR}
  ${BOOST_INCLUDE_DIR}
  )

IF(USING_LOCAL_BOOST)
  ADD_CONVENIENCE_LIBRARY(boost_lib ${BOOST_LIB_SOURCES})

  # Do not build library unless it is needed by some other target.
  SET_PROPERTY(TARGET boost_lib PROPERTY EXCLUDE_FROM_ALL TRUE)

  IF(HAVE_clock_gettime_IN_rt)
    TARGET_LINK_LIBRARIES(boost_lib ${LIBRT})
  ENDIF()

  # Need explicit pthread for gcc -fsanitize=address
  IF(CMAKE_USE_PTHREADS_INIT AND CMAKE_C_FLAGS MATCHES "-fsanitize=")
    TARGET_LINK_LIBRARIES(boost_lib pthread)
  ENDIF()
  SET(LOCAL_BOOST_LIB boost_lib)
ENDIF()

SET(MYSQLPUMP_LIB_SOURCES
  abstract_chain_element.cc
  abstract_connection_provider.cc
  abstract_crawler.cc
  abstract_data_formatter_wrapper.cc
  abstract_data_object.cc
  abstract_database_dump_task.cc
  abstract_dump_task.cc
  abstract_mysql_chain_element_extension.cc
  abstract_object_reader_wrapper.cc
  abstract_output_writer_wrapper.cc
  abstract_plain_sql_object.cc
  abstract_plain_sql_object_dump_task.cc
  abstract_progress_reporter.cc
  abstract_progress_watcher.cc
  abstract_simple_dump_task.cc
  abstract_table_dump_task.cc
  chain_data.cc
  composite_message_handler.cc
  compression_lz4_writer.cc
  compression_zlib_writer.cc
  database.cc
  database_end_dump_task.cc
  database_start_dump_task.cc
  dump_end_dump_task.cc
  dump_start_dump_task.cc
  event_scheduler_event.cc
  field.cc
  file_writer.cc
  i_chain_element.cc
  i_connection_provider.cc
  i_crawler.cc
  i_data_formatter.cc
  i_data_formatter_wrapper.cc
  i_data_object.cc
  i_dump_task.cc
  i_object_reader.cc
  i_object_reader_wrapper.cc
  i_output_writer.cc
  i_output_writer_wrapper.cc
  i_progress_reporter.cc
  i_progress_watcher.cc
  item_processing_data.cc
  mysql_chain_element_options.cc
  mysql_crawler.cc
  mysql_field.cc
  mysql_function.cc
  mysql_object_reader.cc
  mysql_object_reader_options.cc
  mysqldump_tool_chain_maker.cc
  mysqldump_tool_chain_maker_options.cc
  object_filter.cc
  object_queue.cc
  pattern_matcher.cc
  privilege.cc
  row.cc
  row_group_dump_task.cc
  simple_id_generator.cc
  single_transaction_connection_provider.cc
  sql_formatter.cc
  sql_formatter_options.cc
  standard_progress_watcher.cc
  standard_writer.cc
  stored_procedure.cc
  table.cc
  table_deferred_indexes_dump_task.cc
  table_definition_dump_task.cc
  table_rows_dump_task.cc
  tables_definition_ready_dump_task.cc
  thread.cc
  thread_group.cc
  thread_specific_connection_provider.cc
  trigger.cc
  view.cc
  ../../sql-common/my_user.c)
ADD_CONVENIENCE_LIBRARY(mysqlpump_lib ${MYSQLPUMP_LIB_SOURCES})
TARGET_LINK_LIBRARIES(mysqlpump_lib
   client_base ${LZ4_LIBRARY})

# Files that use Boost sources that trigger compilation warnings
SET(MYSQLPUMP_BOOST_SOURCES
  abstract_crawler.cc
  abstract_mysql_chain_element_extension.cc
  abstract_progress_watcher.cc
  mysql_chain_element_options.cc
  mysql_object_reader.cc
  mysqldump_tool_chain_maker.cc
  mysqldump_tool_chain_maker_options.cc
  object_filter.cc
  object_queue.cc
  program.cc
  standard_progress_watcher.cc
  sql_formatter.cc
  table.cc
)

# Boost source has unused local typedefs.
MY_CHECK_CXX_COMPILER_FLAG("-Wunused-local-typedefs" HAVE_UNUSED_TYPEDEFS)
IF(HAVE_UNUSED_TYPEDEFS)
  ADD_COMPILE_FLAGS(${MYSQLPUMP_BOOST_SOURCES}
    COMPILE_FLAGS "-Wno-unused-local-typedefs")
ENDIF()

ADD_COMPILE_FLAGS(
  compression_writer.cc
  mysqldump_tool_chain_maker.cc
  COMPILE_FLAGS -I${LZ4_INCLUDE_DIR}
)
MYSQL_ADD_EXECUTABLE(mysqlpump  program.cc)

TARGET_LINK_LIBRARIES(mysqlpump mysqlpump_lib ${LOCAL_BOOST_LIB})
IF(USING_SYSTEM_BOOST)
  TARGET_LINK_LIBRARIES(mysqlpump "-lboost_system")
  TARGET_LINK_LIBRARIES(mysqlpump "-lboost_chrono")
ENDIF()

SET_TARGET_PROPERTIES(mysqlpump PROPERTIES HAS_CXX TRUE)

# Store the executable in our parent directory.
# Why? In pushbuild we do 'touch client/mysql*' to prevent rebuild,
#      in the case where we have a separate build of libmysqlclient.so
IF (NOT WIN32)
  SET_TARGET_PROPERTIES(mysqlpump
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/client)
ENDIF()


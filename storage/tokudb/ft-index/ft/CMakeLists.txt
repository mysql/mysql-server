include_directories(${CMAKE_CURRENT_SOURCE_DIR})

## generate log_code.cc, log_print.cc, log_header.cc
set_source_files_properties(
  "${CMAKE_CURRENT_BINARY_DIR}/log_code"
  "${CMAKE_CURRENT_BINARY_DIR}/log_print"
  "${CMAKE_CURRENT_BINARY_DIR}/log_header.h"
  PROPERTIES GENERATED TRUE)

add_executable(logformat logger/logformat.cc)
target_link_libraries(logformat ${LIBTOKUPORTABILITY}_static)
add_space_separated_property(TARGET logformat LINK_FLAGS --coverage)


add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/log_code.cc"
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/log_print.cc"
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/log_header.h"
  COMMAND $<TARGET_FILE:logformat> .
  DEPENDS logger/logformat
  )
add_custom_target(
  generate_log_code
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/log_code.cc" "${CMAKE_CURRENT_BINARY_DIR}/log_print.cc" "${CMAKE_CURRENT_BINARY_DIR}/log_header.h"
  )

set(FT_SOURCES
  bndata
  cachetable/background_job_manager
  cachetable/cachetable
  cachetable/checkpoint
  cursor
  ft
  ft-cachetable-wrappers
  ft-flusher
  ft-hot-flusher
  ft-ops
  ft-test-helpers
  ft-verify
  loader/callbacks
  loader/dbufio
  loader/loader
  loader/pqueue
  leafentry
  le-cursor
  logger/logcursor
  logger/logfilemgr
  logger/logger
  logger/log_upgrade
  logger/recover
  msg
  msg_buffer
  node
  pivotkeys
  serialize/block_allocator
  serialize/block_allocator_strategy
  serialize/block_table
  serialize/compress
  serialize/ft_node-serialize
  serialize/ft-node-deserialize
  serialize/ft-serialize
  serialize/quicklz
  serialize/sub_block
  txn/rollback
  txn/rollback-apply
  txn/rollback-ct-callbacks
  txn/rollback_log_node_cache
  txn/roll
  txn/txn
  txn/txn_child_manager
  txn/txn_manager
  txn/xids
  ule
  "${CMAKE_CURRENT_BINARY_DIR}/log_code"
  "${CMAKE_CURRENT_BINARY_DIR}/log_print"
  )

add_library(ft SHARED ${FT_SOURCES})
add_library(ft_static STATIC ${FT_SOURCES})
## we're going to link this into libtokudb.so so it needs to have PIC
set_target_properties(ft_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
maybe_add_gcov_to_libraries(ft ft_static)

## depend on other generated targets
add_dependencies(ft install_tdb_h generate_log_code build_lzma)
add_dependencies(ft_static install_tdb_h generate_log_code build_lzma)

## link with lzma (which should be static) and link dependers with zlib
target_link_libraries(ft LINK_PRIVATE util_static lzma ${LIBTOKUPORTABILITY})
target_link_libraries(ft LINK_PUBLIC z)
target_link_libraries(ft_static LINK_PRIVATE lzma)

add_subdirectory(tests)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

## generate log_code.cc, log_print.cc, log_header.cc
set_source_files_properties(
  "${CMAKE_CURRENT_BINARY_DIR}/log_code"
  "${CMAKE_CURRENT_BINARY_DIR}/log_print"
  "${CMAKE_CURRENT_BINARY_DIR}/log_header.h"
  PROPERTIES GENERATED TRUE)

add_executable(logformat logformat.cc)
target_link_libraries(logformat ${LIBTOKUPORTABILITY}_static)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/log_code.cc"
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/log_print.cc"
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/log_header.h"
  COMMAND $<TARGET_FILE:logformat> .
  DEPENDS logformat
  )
add_custom_target(
  generate_log_code
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/log_code.cc" "${CMAKE_CURRENT_BINARY_DIR}/log_print.cc" "${CMAKE_CURRENT_BINARY_DIR}/log_header.h"
  )

set(FT_SOURCES
  background_job_manager
  block_allocator
  block_table
  bndata
  cachetable
  checkpoint
  compress
  dbufio
  fifo
  ft
  ft-cachetable-wrappers
  ft-flusher
  ft-hot-flusher
  ftloader
  ftloader-callback
  ft_msg
  ft_node-serialize
  ft-node-deserialize
  ft-ops
  ft-serialize
  ft-test-helpers
  ft-verify
  key
  leafentry
  le-cursor
  logcursor
  logfilemgr
  logger
  log_upgrade
  memarena
  minicron
  omt
  pqueue
  queue
  quicklz
  recover
  rollback
  rollback-apply
  rollback-ct-callbacks
  rollback_log_node_cache
  roll
  sub_block
  txn
  txn_child_manager
  txn_manager
  ule
  x1764
  xids
  ybt
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
target_link_libraries(ft LINK_PUBLIC ${ZLIB_LIBRARY} )
target_link_libraries(ft_static LINK_PRIVATE lzma)

## build the bins in this directory
foreach(tool tokuftdump tdb_logprint tdb-recover ftverify)
  add_executable(${tool} ${tool}.cc)
  add_dependencies(${tool} install_tdb_h)
  target_link_libraries(${tool} ft_static util_static ${ZLIB_LIBRARY} lzma ${LIBTOKUPORTABILITY}_static ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_SYSTEM_LIBS})
  add_space_separated_property(TARGET ${tool} COMPILE_FLAGS -fvisibility=hidden)
endforeach(tool)

# link in math.h library just for this tool.
target_link_libraries(ftverify m)

install(
  TARGETS tokuftdump
  COMPONENT Server
  DESTINATION ${INSTALL_BINDIR}
  )

add_subdirectory(tests)

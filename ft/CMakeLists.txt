include_directories(${CMAKE_CURRENT_SOURCE_DIR})

## generate log_code.c, log_print.c, log_header.c
set_source_files_properties(
  ${CMAKE_CURRENT_SOURCE_DIR}/log_code.c
  ${CMAKE_CURRENT_SOURCE_DIR}/log_print.c
  ${CMAKE_CURRENT_SOURCE_DIR}/log_header.h
  PROPERTIES GENERATED TRUE)

add_executable(logformat logformat.c)
add_space_separated_property(TARGET logformat COMPILE_FLAGS "-fvisibility=hidden -fPIE")
add_space_separated_property(TARGET logformat LINK_FLAGS -pie)
target_link_libraries(logformat ${LIBTOKUPORTABILITY})

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/log_code.c"
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/log_print.c"
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/log_header.h"
  COMMAND $<TARGET_FILE:logformat> .
  DEPENDS logformat
  )
add_custom_target(
  generate_log_code
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/log_code.c" "${CMAKE_CURRENT_BINARY_DIR}/log_print.c" "${CMAKE_CURRENT_BINARY_DIR}/log_header.h"
  )

set(FT_SOURCES
  block_allocator.c
  block_table.c
  cachetable.c
  checkpoint.c
  compress.c
  dbufio.c
  fifo.c
  ft.c
  ft-cachetable-wrappers.c
  ft-flusher.c
  ft-hot-flusher.c
  ftloader.c
  ftloader-callback.c
  ft_msg.c
  ft_node-serialize.c
  ft-node-deserialize.c
  ft-ops.c
  ft-serialize.c
  ft-test-helpers.c
  ft-verify.c
  key.c
  kibbutz.c
  leafentry.c
  le-cursor.c
  logcursor.c
  logfilemgr.c
  logger.c
  log_upgrade.c
  memarena.c
  mempool.c
  minicron.c
  omt.c
  pqueue.c
  queue.c
  quicklz.c
  recover.c
  rollback.c
  rollback-apply.c
  rollback-ct-callbacks.c
  roll.c
  sort.c
  sub_block.c
  threadpool.c
  txn.c
  txn_manager.c
  ule.c
  workqueue.c
  x1764.c
  xids.c
  ybt.c
  "${CMAKE_CURRENT_BINARY_DIR}/log_code.c"
  "${CMAKE_CURRENT_BINARY_DIR}/log_header.h"
  "${CMAKE_CURRENT_BINARY_DIR}/log_print.c"
  )

add_library(ft SHARED ${FT_SOURCES})
add_library(ft_static STATIC ${FT_SOURCES})
## we're going to link this into libtokudb.so so it needs to have PIC
add_space_separated_property(TARGET ft_static COMPILE_FLAGS -fPIC)
maybe_add_gcov_to_libraries(ft ft_static)

## depend on other generated targets
add_dependencies(ft install_tdb_h build_lzma)
add_dependencies(ft_static install_tdb_h build_lzma)

## link with lzma (which should be static) and link dependers with zlib
target_link_libraries(ft LINK_PRIVATE lzma)
target_link_libraries(ft LINK_INTERFACE_LIBRARIES ${ZLIB_LIBRARIES})
target_link_libraries(ft_static LINK_PRIVATE lzma)

if (CMAKE_C_COMPILER_ID STREQUAL Intel)
  target_link_libraries(ft LINK_PRIVATE -nodefaultlibs)
endif ()

## conditionally use cilk
if (USE_CILK)
  set_property(TARGET ft APPEND PROPERTY COMPILE_DEFINITIONS HAVE_CILK=1)
  set_property(TARGET ft_static APPEND PROPERTY COMPILE_DEFINITIONS HAVE_CILK=1)
  target_link_libraries(ft cilkrts)
  target_link_libraries(ft_static cilkrts)
endif (USE_CILK)

## build the bins in this directory
set(bins
  ftdump
  tdb_logprint
  tdb-recover
  ftverify
  )
foreach(bin ${bins})
  add_executable(${bin} ${bin}.c)
  add_dependencies(${bin} install_tdb_h)
  add_space_separated_property(TARGET ${bin} COMPILE_FLAGS "-fvisibility=hidden -fPIE")
  add_space_separated_property(TARGET ${bin} LINK_FLAGS -pie)
  target_link_libraries(${bin} ft ${LIBTOKUPORTABILITY})

  add_executable(${bin}_static ${bin}.c)
  add_dependencies(${bin}_static install_tdb_h)
  add_space_separated_property(TARGET ${bin}_static COMPILE_FLAGS "-fvisibility=hidden -fPIE")
  add_space_separated_property(TARGET ${bin}_static LINK_FLAGS -pie)
  target_link_libraries(${bin}_static ft_static ${ZLIB_LIBRARIES} ${LIBTOKUPORTABILITY}_static ${CMAKE_THREAD_LIBS_INIT} dl)
endforeach(bin)

# link in math.h library just for this tool.
target_link_libraries(ftverify m)
target_link_libraries(ftverify_static m)

install(
  TARGETS ftdump_static
  DESTINATION bin
  )

add_subdirectory(tests)

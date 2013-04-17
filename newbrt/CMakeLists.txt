include_directories(${CMAKE_CURRENT_SOURCE_DIR})

## generate log_code.c, log_print.c, log_header.c
## TODO: generate these in the build directory instead
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/log_code.c PROPERTIES GENERATED TRUE)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/log_print.c PROPERTIES GENERATED TRUE)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/log_header.h PROPERTIES GENERATED TRUE)

add_executable(logformat logformat.c)
target_link_libraries(logformat ${LIBTOKUPORTABILITY})

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/log_code.c"
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/log_print.c"
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/log_header.h"
  COMMAND $<TARGET_FILE:logformat> .
  DEPENDS logformat
  )

set(BRT_SOURCES
  block_allocator.c
  block_table.c
  brtloader.c
  brtloader-callback.c
  brt-serialize.c
  brt-verify.c
  brt.c
  brt-cachetable-wrappers.c
  brt-flusher.c
  brt_header.c
  brt-hot-flusher.c
  brt_msg.c
  brt-test-helpers.c
  cachetable.c
  checkpoint.c
  compress.c
  dbufio.c
  fifo.c
  key.c
  kibbutz.c
  leafentry.c
  le-cursor.c
  logfilemgr.c
  logger.c
  "${CMAKE_CURRENT_BINARY_DIR}/log_code.c"
  "${CMAKE_CURRENT_BINARY_DIR}/log_header.h"
  log_upgrade.c
  "${CMAKE_CURRENT_BINARY_DIR}/log_print.c"
  logcursor.c
  memarena.c
  mempool.c
  minicron.c
  omt.c
  pqueue.c
  queue.c
  quicklz.c
  recover.c
  roll.c
  rollback.c
  sort.c
  sub_block.c
  ule.c
  threadpool.c
  txn.c
  workqueue.c
  x1764.c
  xids.c
  ybt.c
  )

add_library(newbrt SHARED ${BRT_SOURCES})
add_library(newbrt_static STATIC ${BRT_SOURCES})
## we're going to link this into libtokudb.so so it needs to have PIC
set_property(TARGET newbrt_static APPEND PROPERTY COMPILE_FLAGS "-fPIC")
maybe_add_gcov_to_libraries(newbrt newbrt_static)

## depend on other generated targets
add_dependencies(newbrt install_tdb_h build_lzma)
add_dependencies(newbrt_static install_tdb_h build_lzma)

## link with tokuportability, and lzma (which should be static)
target_link_libraries(newbrt ${LIBTOKUPORTABILITY} lzma)
target_link_libraries(newbrt_static ${LIBTOKUPORTABILITY} lzma)

if (CMAKE_C_COMPILER_ID STREQUAL Intel)
  ## don't link with default libs, those come with tokuportability, but we
  ## do need libc and we need the intel libirc (and it should be static to
  ## be redistributable)
  target_link_libraries(newbrt -nodefaultlibs c -Bstatic irc -Bdynamic)
endif ()

## conditionally use cilk
if (USE_CILK)
  set_property(TARGET newbrt APPEND PROPERTY COMPILE_DEFINITIONS HAVE_CILK=1)
  set_property(TARGET newbrt_static APPEND PROPERTY COMPILE_DEFINITIONS HAVE_CILK=1)
  target_link_libraries(newbrt cilkrts)
  target_link_libraries(newbrt_static cilkrts)
endif (USE_CILK)

## build the bins in this directory
set(bins
  brtdump
  tdb_logprint
  tdb-recover
  )
foreach(bin ${bins})
  add_executable(${bin} ${bin}.c)
  add_dependencies(${bin} install_tdb_h)
  target_link_libraries(${bin} newbrt ${LIBTOKUPORTABILITY})

  add_executable(${bin}_static ${bin}.c)
  add_dependencies(${bin} install_tdb_h)
  target_link_libraries(${bin}_static newbrt_static ${LIBTOKUPORTABILITY_STATIC})
endforeach(bin)

install(
  TARGETS brtdump_static
  DESTINATION bin
  )

add_subdirectory(tests)

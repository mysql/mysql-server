include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(range_tree)
add_subdirectory(lock_tree)

set(tokudb_srcs
  ydb_lib.c
  ydb.c
  ydb_cursor.c
  ydb_db.c
  ydb_env_func.c
  ydb_row_lock.c
  ydb_txn.c
  ydb_write.c
  errors.c
  loader.c
  indexer.c
  indexer-undo-do.c
  elocks.c
  )

## make the shared library
add_library(${LIBTOKUDB} SHARED ${tokudb_srcs})
add_dependencies(${LIBTOKUDB} generate_logging_code install_tdb_h)
target_link_libraries(${LIBTOKUDB} lock_tree_static range_tree_static newbrt_static)

## make the static library
add_library(${LIBTOKUDB}_static STATIC ${tokudb_srcs})
add_dependencies(${LIBTOKUDB}_static generate_logging_code install_tdb_h)
target_link_libraries(${LIBTOKUDB}_static lock_tree_static range_tree_static newbrt_static)

## add a version script and set -fvisibility=hidden for the shared library
configure_file(export.map . COPYONLY)
get_target_property(link_flags ${LIBTOKUDB} LINK_FLAGS)
set_target_properties(${LIBTOKUDB} PROPERTIES
  LINK_FLAGS "${LINK_FLAGS} -Wl,--version-script=export.map")
set_targets_visibility_hidden(${LIBTOKUDB})

## add gcov and define _GNU_SOURCE
maybe_add_gcov_to_libraries(${LIBTOKUDB} ${LIBTOKUDB}_static)
set_property(TARGET ${LIBTOKUDB} ${LIBTOKUDB}_static APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

if (CMAKE_C_COMPILER_ID STREQUAL Intel)
  ## don't link with default libs, those come with tokuportability, but we
  ## do need libc and we need the intel libirc (and it should be static to
  ## be redistributable)
  target_link_libraries(${LIBTOKUDB} -nodefaultlibs c -Bstatic irc -Bdynamic)
endif ()

install(
  TARGETS ${LIBTOKUDB}
  DESTINATION lib
  )

add_subdirectory(tests)

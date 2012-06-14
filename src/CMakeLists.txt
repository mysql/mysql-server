include_directories(${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_BINARY_DIR}/..")

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
  )

## make the shared library
add_library(${LIBTOKUDB} SHARED ${tokudb_srcs})
add_dependencies(${LIBTOKUDB} install_tdb_h)
target_link_libraries(${LIBTOKUDB} LINK_PRIVATE lock_tree_static range_tree_static ft_static ${LIBTOKUPORTABILITY})
target_link_libraries(${LIBTOKUDB} LINK_PUBLIC z)

## make the static library
add_library(${LIBTOKUDB}_static STATIC ${tokudb_srcs})
add_dependencies(${LIBTOKUDB}_static install_tdb_h)
add_space_separated_property(TARGET ${LIBTOKUDB}_static COMPILE_FLAGS -fPIC)
target_link_libraries(${LIBTOKUDB}_static LINK_PRIVATE lock_tree_static range_tree_static ft_static)

if (CMAKE_C_COMPILER_ID STREQUAL Intel)
  target_link_libraries(${LIBTOKUDB} LINK_PRIVATE -nodefaultlibs)
  target_link_libraries(${LIBTOKUDB}_static LINK_PRIVATE -nodefaultlibs)
endif ()

## add a version script and set -fvisibility=hidden for the shared library
configure_file(export.map . COPYONLY)
if (NOT CMAKE_SYSTEM_NAME STREQUAL Darwin)
  add_space_separated_property(TARGET ${LIBTOKUDB} COMPILE_FLAGS_RELEASE -fvisibility=hidden)
  add_space_separated_property(TARGET ${LIBTOKUDB} LINK_FLAGS_RELEASE "-Wl,--version-script=export.map")
endif ()

## add gcov and define _GNU_SOURCE
maybe_add_gcov_to_libraries(${LIBTOKUDB} ${LIBTOKUDB}_static)
set_property(TARGET ${LIBTOKUDB} ${LIBTOKUDB}_static APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

set_targets_need_intel_libs(${LIBTOKUDB})

install(
  TARGETS ${LIBTOKUDB}
  DESTINATION lib
  )

add_subdirectory(tests)

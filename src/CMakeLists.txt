include_directories(${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_BINARY_DIR}/..")

add_subdirectory(range_tree)
add_subdirectory(lock_tree)

set(tokudb_srcs
  ydb
  ydb_cursor
  ydb_db
  ydb_env_func
  ydb_row_lock
  ydb_txn
  ydb_write
  errors
  loader
  indexer
  indexer-undo-do
  )

## make the shared library
add_library(${LIBTOKUDB} SHARED ${tokudb_srcs})
add_dependencies(${LIBTOKUDB} install_tdb_h generate_log_code)
target_link_libraries(${LIBTOKUDB} LINK_PRIVATE lock_tree_static range_tree_static ft_static lzma ${LIBTOKUPORTABILITY} util_static)
target_link_libraries(${LIBTOKUDB} LINK_PUBLIC z)

## make the static library
add_library(tokudb_static_conv STATIC ${tokudb_srcs})
add_dependencies(tokudb_static_conv install_tdb_h generate_log_code)
set_target_properties(tokudb_static_conv PROPERTIES POSITION_INDEPENDENT_CODE ON)
set(tokudb_source_libs tokudb_static_conv lock_tree_static range_tree_static ft_static lzma ${LIBTOKUPORTABILITY} util_static z ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_SYSTEM_LIBS})
merge_static_libs(${LIBTOKUDB}_static ${LIBTOKUDB}_static "${tokudb_source_libs}")

## add a version script and set -fvisibility=hidden for the shared library
configure_file(export.map . COPYONLY)
if (NOT CMAKE_SYSTEM_NAME STREQUAL Darwin)
  add_space_separated_property(TARGET ${LIBTOKUDB} COMPILE_FLAGS -fvisibility=hidden -fvisibility-inlines-hidden)
  add_space_separated_property(TARGET ${LIBTOKUDB} LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/export.map")
endif ()

## add gcov and define _GNU_SOURCE
maybe_add_gcov_to_libraries(${LIBTOKUDB} ${LIBTOKUDB}_static)
set_property(TARGET ${LIBTOKUDB} ${LIBTOKUDB}_static APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

install(
  TARGETS ${LIBTOKUDB} ${LIBTOKUDB}_static
  DESTINATION lib
  )

add_subdirectory(tests)

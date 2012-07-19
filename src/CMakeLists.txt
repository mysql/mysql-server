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
target_link_libraries(${LIBTOKUDB} LINK_PRIVATE lock_tree_static range_tree_static ft_static lzma ${LIBTOKUPORTABILITY})
target_link_libraries(${LIBTOKUDB} LINK_PUBLIC z)

## make the static library
add_library(tokudb_static_conv STATIC ${tokudb_srcs})
add_dependencies(tokudb_static_conv install_tdb_h generate_log_code)
add_space_separated_property(TARGET tokudb_static_conv COMPILE_FLAGS -fPIC)
set(tokudb_required_libs tokudb_static_conv lock_tree_static range_tree_static
    ft_static lzma ${LIBTOKUPORTABILITY} z ${CMAKE_THREAD_LIBS_INIT} dl)
if (CMAKE_CXX_COMPILER_ID STREQUAL Intel)
  find_library(IRC_LIBRARIES NAMES libirc.a irc PATHS ENV LIBRARY_PATH)
  add_library(irc_fullpath STATIC IMPORTED)
  set_target_properties(irc_fullpath PROPERTIES IMPORTED_LOCATION ${IRC_LIBRARIES})
  list(APPEND tokudb_required_libs irc_fullpath)
endif ()
set(CMAKE_TOP_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/..")
merge_static_libs(${LIBTOKUDB}_static ${LIBTOKUDB}_static "${tokudb_required_libs}")

if (CMAKE_CXX_COMPILER_ID STREQUAL Intel)
  target_link_libraries(${LIBTOKUDB} LINK_PRIVATE -nodefaultlibs)
endif ()

## add a version script and set -fvisibility=hidden for the shared library
configure_file(export.map . COPYONLY)
if (NOT CMAKE_SYSTEM_NAME STREQUAL Darwin)
  add_space_separated_property(TARGET ${LIBTOKUDB} COMPILE_FLAGS -fvisibility=hidden -fvisibility-inlines-hidden)
  add_space_separated_property(TARGET ${LIBTOKUDB} LINK_FLAGS "-Wl,--version-script=export.map")
endif ()

## add gcov and define _GNU_SOURCE
maybe_add_gcov_to_libraries(${LIBTOKUDB} tokudb_static_conv)
set_property(TARGET ${LIBTOKUDB} tokudb_static_conv APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

set_targets_need_intel_libs(${LIBTOKUDB})

install(
  TARGETS ${LIBTOKUDB} ${LIBTOKUDB}_static
  DESTINATION lib
  )

add_subdirectory(tests)

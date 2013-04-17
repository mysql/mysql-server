include_directories(${CMAKE_CURRENT_SOURCE_DIR})

## make the linear library for tests
add_library(range_tree_lin SHARED linear.c)
target_link_libraries(range_tree_lin ft ${LIBTOKUPORTABILITY})
add_dependencies(range_tree_lin install_tdb_h)

## make the log library for tests
add_library(range_tree_tlog SHARED log_nooverlap.c)
target_link_libraries(range_tree_tlog ft ${LIBTOKUPORTABILITY})
add_dependencies(range_tree_tlog install_tdb_h)

set_targets_need_intel_libs(range_tree_lin range_tree_tlog)
if (CMAKE_C_COMPILER_ID STREQUAL Intel)
  add_space_separated_property(TARGET range_tree_lin LINK_FLAGS "-diag-disable 10237")
  add_space_separated_property(TARGET range_tree_tlog LINK_FLAGS "-diag-disable 10237")
endif ()

## make the real library, it's going to go into libtokudb.so so it needs
## to be PIC
add_library(range_tree_static STATIC log_nooverlap.c)
add_space_separated_property(TARGET range_tree_static COMPILE_FLAGS "-fvisibility=hidden -fPIC")
add_dependencies(range_tree_static install_tdb_h)

maybe_add_gcov_to_libraries(range_tree_lin range_tree_tlog range_tree_static)

add_subdirectory(tests)

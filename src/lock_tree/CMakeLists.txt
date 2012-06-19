include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(lock_tree_srcs locktree.c idlth.c lth.c rth.c txnid_set.c wfg.c)

## make the linear library for tests
add_library(lock_tree_lin SHARED ${lock_tree_srcs})
target_link_libraries(lock_tree_lin range_tree_lin)
add_dependencies(lock_tree_lin install_tdb_h)

## make the log library for tests
add_library(lock_tree_tlog SHARED ${lock_tree_srcs})
set_property(TARGET lock_tree_tlog APPEND PROPERTY
  COMPILE_DEFINITIONS TOKU_RT_NOOVERLAPS)
target_link_libraries(lock_tree_tlog range_tree_tlog)
add_dependencies(lock_tree_tlog install_tdb_h)

set_targets_need_intel_libs(lock_tree_lin lock_tree_tlog)

## make the real library, it's going to go into libtokudb.so so it needs
## to be PIC
add_library(lock_tree_static STATIC ${lock_tree_srcs})
add_space_separated_property(TARGET lock_tree_static COMPILE_FLAGS "-fvisibility=hidden -fPIC")
set_property(TARGET lock_tree_static APPEND PROPERTY
  COMPILE_DEFINITIONS TOKU_RT_NOOVERLAPS)
add_dependencies(lock_tree_static install_tdb_h)

maybe_add_gcov_to_libraries(lock_tree_lin lock_tree_tlog lock_tree_static)

add_subdirectory(tests)

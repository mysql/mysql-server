include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(lock_tree_srcs locktree idlth lth rth txnid_set wfg)

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

## make the real library, it's going to go into libtokudb.so so it needs
## to be PIC
add_library(lock_tree_objects OBJECT ${lock_tree_srcs})
set_target_properties(lock_tree_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_space_separated_property(TARGET lock_tree_objects COMPILE_FLAGS "-fvisibility=hidden")
set_property(TARGET lock_tree_objects APPEND PROPERTY
  COMPILE_DEFINITIONS TOKU_RT_NOOVERLAPS)
add_dependencies(lock_tree_objects install_tdb_h)

add_library(lock_tree_static STATIC $<TARGET_OBJECTS:lock_tree_objects>)
maybe_add_gcov_to_libraries(lock_tree_lin lock_tree_tlog lock_tree_static)

add_subdirectory(tests)

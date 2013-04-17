include_directories(${CMAKE_CURRENT_SOURCE_DIR})

## make the linear library for tests
add_library(range_tree_lin SHARED linear)
target_link_libraries(range_tree_lin ft ${LIBTOKUPORTABILITY})
add_dependencies(range_tree_lin install_tdb_h)

## make the log library for tests
add_library(range_tree_tlog SHARED log_nooverlap)
target_link_libraries(range_tree_tlog ft ${LIBTOKUPORTABILITY})
add_dependencies(range_tree_tlog install_tdb_h)

## make the real library, it's going to go into libtokudb.so so it needs
## to be PIC
add_library(range_tree_objects OBJECT log_nooverlap)
set_target_properties(range_tree_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_space_separated_property(TARGET range_tree_objects COMPILE_FLAGS "-fvisibility=hidden")
add_dependencies(range_tree_objects install_tdb_h)

maybe_add_gcov_to_libraries(range_tree_lin range_tree_tlog range_tree_objects)

add_library(range_tree_static STATIC $<TARGET_OBJECTS:range_tree_objects>)

add_subdirectory(tests)

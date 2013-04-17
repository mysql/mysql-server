include_directories(${CMAKE_CURRENT_SOURCE_DIR})

## make the linear library
add_library(range_tree_lin_static STATIC linear.c)
add_dependencies(range_tree_lin_static install_tdb_h)

## make the log library
add_library(range_tree_tlog_static STATIC log_nooverlap.c)
add_dependencies(range_tree_tlog_static install_tdb_h)

## make the real library, it's going to go into libtokudb.so so it needs
## to be PIC
add_library(range_tree_static STATIC log_nooverlap.c)
add_space_separated_property(TARGET range_tree_static COMPILE_FLAGS -fPIC)
add_space_separated_property(TARGET range_tree_static COMPILE_FLAGS_RELEASE -fvisibility=hidden)
add_dependencies(range_tree_static install_tdb_h)

maybe_add_gcov_to_libraries(range_tree_lin_static range_tree_tlog_static range_tree_static)

add_subdirectory(tests)

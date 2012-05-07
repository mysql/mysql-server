include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(range_tree_lin_static STATIC linear.c)
set_property(TARGET range_tree_lin_static APPEND PROPERTY
  COMPILE_FLAGS "-fPIC")
add_dependencies(range_tree_lin_static install_tdb_h)

add_library(range_tree_tlog_static STATIC log_nooverlap.c)
set_property(TARGET range_tree_tlog_static APPEND PROPERTY
  COMPILE_FLAGS "-fPIC")
add_dependencies(range_tree_tlog_static install_tdb_h)

add_library(range_tree_static STATIC log_nooverlap.c)
set_property(TARGET range_tree_static APPEND PROPERTY
  COMPILE_FLAGS "-fPIC")
set_property(SOURCE log_nooverlap.c APPEND PROPERTY LABELS RUN_GCOV)
add_dependencies(range_tree_static install_tdb_h)

maybe_add_gcov_to_libraries(range_tree_lin_static range_tree_tlog_static range_tree_static)

add_subdirectory(tests)

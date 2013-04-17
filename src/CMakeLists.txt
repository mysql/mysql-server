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

add_library(${LIBTOKUDB} SHARED ${tokudb_srcs})
add_dependencies(${LIBTOKUDB} generate_logging_code install_tdb_h)
target_link_libraries(${LIBTOKUDB} lock_tree_static range_tree_static newbrt_static)

add_library(${LIBTOKUDB}_static STATIC ${tokudb_srcs})
add_dependencies(${LIBTOKUDB}_static generate_logging_code install_tdb_h)
target_link_libraries(${LIBTOKUDB}_static lock_tree_static range_tree_static newbrt_static)

maybe_add_gcov_to_libraries(${LIBTOKUDB} ${LIBTOKUDB}_static)

install(
  TARGETS ${LIBTOKUDB}
  DESTINATION lib
  )

add_subdirectory(tests)

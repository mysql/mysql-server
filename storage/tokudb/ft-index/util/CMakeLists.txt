set(util_srcs
  context
  dbt
  frwlock
  kibbutz
  memarena
  mempool
  minicron
  partitioned_counter
  queue
  threadpool
  scoped_malloc
  x1764
  )

add_library(util SHARED ${util_srcs})
add_library(util_static STATIC ${util_srcs})
maybe_add_gcov_to_libraries(util util_static)
set_target_properties(util_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(util LINK_PRIVATE ${LIBTOKUPORTABILITY})
target_link_libraries(util LINK_PUBLIC ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_SYSTEM_LIBS})
add_dependencies(util install_tdb_h)
add_dependencies(util_static install_tdb_h)

# detect when we are being built as a subproject
if (NOT DEFINED MYSQL_PROJECT_NAME_DOCSTRING)
  install(
    FILES partitioned_counter.h
    DESTINATION include
    COMPONENT tokukv_headers
    )
endif ()

add_subdirectory(tests)

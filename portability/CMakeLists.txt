include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(tokuportability_srcs
  huge_page_detection
  file
  memory
  os_malloc
  portability
  toku_assert
  toku_fair_rwlock
  toku_path
  toku_pthread
  toku_time
  )

add_library(${LIBTOKUPORTABILITY} SHARED ${tokuportability_srcs})
target_link_libraries(${LIBTOKUPORTABILITY} LINK_PRIVATE jemalloc)
target_link_libraries(${LIBTOKUPORTABILITY} LINK_PUBLIC ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_SYSTEM_LIBS})

add_library(tokuportability_static_conv STATIC ${tokuportability_srcs})
set_target_properties(tokuportability_static_conv PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_dependencies(tokuportability_static_conv build_jemalloc)
set(tokuportability_source_libs tokuportability_static_conv jemalloc ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_SYSTEM_LIBS})
toku_merge_static_libs(${LIBTOKUPORTABILITY}_static ${LIBTOKUPORTABILITY}_static "${tokuportability_source_libs}")

maybe_add_gcov_to_libraries(${LIBTOKUPORTABILITY} tokuportability_static_conv)
set_property(TARGET ${LIBTOKUPORTABILITY} tokuportability_static_conv APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

set_property(SOURCE file memory os_malloc portability toku_assert toku_rwlock APPEND PROPERTY
  COMPILE_DEFINITIONS TOKU_ALLOW_DEPRECATED=1)

install(
  FILES toku_os_types.h toku_time.h
  DESTINATION include
  )
install(
  TARGETS ${LIBTOKUPORTABILITY} ${LIBTOKUPORTABILITY}_static
  DESTINATION lib
  )

add_subdirectory(tests)

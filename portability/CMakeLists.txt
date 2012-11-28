include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(tokuportability_srcs
  file
  memory
  os_malloc
  portability
  toku_assert
  toku_pthread
  toku_fair_rwlock
  )

add_library(${LIBTOKUPORTABILITY} SHARED ${tokuportability_srcs})
add_library(${LIBTOKUPORTABILITY}_static STATIC ${tokuportability_srcs})
maybe_add_gcov_to_libraries(${LIBTOKUPORTABILITY} ${LIBTOKUPORTABILITY}_static)
set_property(TARGET ${LIBTOKUPORTABILITY} ${LIBTOKUPORTABILITY}_static APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)
set_target_properties(${LIBTOKUPORTABILITY}_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${LIBTOKUPORTABILITY} LINK_PUBLIC rt ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_SYSTEM_LIBS})

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

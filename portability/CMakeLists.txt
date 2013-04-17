include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(tokuportability_srcs
  file.c
  memory.c
  os_malloc.c
  portability.c
  toku_assert.c
  toku_pthread.c
  toku_rwlock.c
  )

add_library(${LIBTOKUPORTABILITY} SHARED ${tokuportability_srcs})
add_library(${LIBTOKUPORTABILITY}_static STATIC ${tokuportability_srcs})
maybe_add_gcov_to_libraries(${LIBTOKUPORTABILITY} ${LIBTOKUPORTABILITY}_static)
set_property(TARGET ${LIBTOKUPORTABILITY} ${LIBTOKUPORTABILITY}_static APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

foreach(src file.c memory.c os_malloc.c portability.c toku_assert.c toku_rwlock.c)
  set_property(SOURCE ${src} APPEND PROPERTY COMPILE_DEFINITIONS TOKU_ALLOW_DEPRECATED=1)
endforeach(src)

install(
  FILES toku_os_types.h toku_time.h
  DESTINATION include
  )
install(
  TARGETS ${LIBTOKUPORTABILITY} ${LIBTOKUPORTABILITY}_static
  DESTINATION lib
  )

add_subdirectory(tests)
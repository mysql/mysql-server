include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(tokuportability_srcs
  file.c
  memory.c
  os_malloc.c
  portability.c
  toku_assert.c
  toku_pthread.c
  toku_fair_rwlock.c
  )

add_library(${LIBTOKUPORTABILITY} SHARED ${tokuportability_srcs})
add_library(${LIBTOKUPORTABILITY}_static STATIC ${tokuportability_srcs})
maybe_add_gcov_to_libraries(${LIBTOKUPORTABILITY} ${LIBTOKUPORTABILITY}_static)
set_property(TARGET ${LIBTOKUPORTABILITY} ${LIBTOKUPORTABILITY}_static APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)
add_space_separated_property(TARGET ${LIBTOKUPORTABILITY}_static COMPILE_FLAGS -fPIC)
target_link_libraries(${LIBTOKUPORTABILITY} LINK_INTERFACE_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} dl)

if (CMAKE_C_COMPILER_ID STREQUAL Intel)
  ## ignore warning about cilkrts linking dynamically
  add_space_separated_property(TARGET ${LIBTOKUPORTABILITY} LINK_FLAGS "-diag-disable 10237")
endif ()

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

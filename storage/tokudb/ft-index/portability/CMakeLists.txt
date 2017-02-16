include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(tokuportability_srcs
  huge_page_detection
  file
  memory
  os_malloc
  portability
  toku_assert
  toku_crash
  toku_path
  toku_pthread
  toku_time
  )

add_library(${LIBTOKUPORTABILITY} SHARED ${tokuportability_srcs})
target_link_libraries(${LIBTOKUPORTABILITY} LINK_PRIVATE ${LIBJEMALLOC})
target_link_libraries(${LIBTOKUPORTABILITY} LINK_PUBLIC ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_SYSTEM_LIBS})

add_library(tokuportability_static_conv STATIC ${tokuportability_srcs})
set_target_properties(tokuportability_static_conv PROPERTIES POSITION_INDEPENDENT_CODE ON)
set(tokuportability_source_libs tokuportability_static_conv ${LIBJEMALLOC} ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_SYSTEM_LIBS})
toku_merge_static_libs(${LIBTOKUPORTABILITY}_static ${LIBTOKUPORTABILITY}_static "${tokuportability_source_libs}")

maybe_add_gcov_to_libraries(${LIBTOKUPORTABILITY} tokuportability_static_conv)
set_property(TARGET ${LIBTOKUPORTABILITY} tokuportability_static_conv APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

set_property(SOURCE file memory os_malloc portability toku_assert toku_rwlock APPEND PROPERTY
  COMPILE_DEFINITIONS TOKU_ALLOW_DEPRECATED=1)

configure_file(toku_config.h.in toku_config.h)
add_custom_target(generate_config_h DEPENDS
  "${CMAKE_CURRENT_BINARY_DIR}/toku_config.h")

# detect when we are being built as a subproject
if (NOT DEFINED MYSQL_PROJECT_NAME_DOCSTRING)
  install(
    FILES toku_os_types.h toku_time.h toku_list.h toku_os.h
    DESTINATION include
    COMPONENT tokukv_headers
    )
  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/toku_config.h"
    DESTINATION include
    COMPONENT tokukv_headers
    )
  install(
    TARGETS ${LIBTOKUPORTABILITY}_static
    DESTINATION ${INSTALL_LIBDIR}
    COMPONENT tokukv_libs_static
    )
  install(
    TARGETS ${LIBTOKUPORTABILITY}
    DESTINATION ${INSTALL_LIBDIR}
    COMPONENT tokukv_libs_shared
    )
endif ()

add_subdirectory(tests)

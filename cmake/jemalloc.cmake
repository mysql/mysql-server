INCLUDE(ExternalProject)

MACRO (USE_BUNDLED_JEMALLOC)
  SET(SOURCE_DIR "${CMAKE_SOURCE_DIR}/extra/jemalloc")
  SET(BINARY_DIR "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/extra/jemalloc/build")
  SET(LIBJEMALLOC "libjemalloc")
  SET(JEMALLOC_CONFIGURE_OPTS "CC=${CMAKE_C_COMPILER}" "--with-private-namespace=jemalloc_internal_" "--enable-cc-silence")
  IF (CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT APPLE) # see the comment in CMakeLists.txt
    LIST(APPEND JEMALLOC_CONFIGURE_OPTS --enable-debug)
  ENDIF()
  ExternalProject_Add(jemalloc
    PREFIX extra/jemalloc
    SOURCE_DIR ${SOURCE_DIR}
    BINARY_DIR ${BINARY_DIR}
    STAMP_DIR  ${SOURCE_DIR}
    CONFIGURE_COMMAND "${SOURCE_DIR}/configure" ${JEMALLOC_CONFIGURE_OPTS}
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} "build_lib_static"
    INSTALL_COMMAND ""
  )
  ADD_LIBRARY(libjemalloc STATIC IMPORTED)
  SET_TARGET_PROPERTIES(libjemalloc PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/lib/libjemalloc_pic.a")
  ADD_DEPENDENCIES(libjemalloc jemalloc)
ENDMACRO()

SET(WITH_JEMALLOC "yes" CACHE STRING
    "Which jemalloc to use (possible values are 'no', 'bundled', 'yes' (same as bundled)")
#"Which jemalloc to use (possible values are 'no', 'bundled', 'system', 'yes' (system if possible, otherwise bundled)")

MACRO (CHECK_JEMALLOC)
  IF(WITH_JEMALLOC STREQUAL "bundled" OR WITH_JEMALLOC STREQUAL "yes")
    USE_BUNDLED_JEMALLOC()
  ENDIF()
ENDMACRO()

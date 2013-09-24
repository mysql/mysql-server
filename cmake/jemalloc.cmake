# old cmake does not have ExternalProject file
IF(CMAKE_VERSION VERSION_LESS "2.8.6")
  MACRO (CHECK_JEMALLOC)
  ENDMACRO()
  RETURN()
ENDIF()

INCLUDE(ExternalProject)

MACRO (USE_BUNDLED_JEMALLOC)
  SET(SOURCE_DIR "${CMAKE_SOURCE_DIR}/extra/jemalloc")
  SET(BINARY_DIR "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/extra/jemalloc/build")
  SET(LIBJEMALLOC "libjemalloc")
  SET(JEMALLOC_CONFIGURE_OPTS "CC=${CMAKE_C_COMPILER} ${CMAKE_C_COMPILER_ARG1}" "--with-private-namespace=jemalloc_internal_" "--enable-cc-silence")
  IF (CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT APPLE) # see the comment in CMakeLists.txt
    LIST(APPEND JEMALLOC_CONFIGURE_OPTS --enable-debug)
  ENDIF()
  
  IF(CMAKE_GENERATOR MATCHES "Makefiles")
    SET(MAKE_COMMAND ${CMAKE_MAKE_PROGRAM})
  ELSE() # Xcode/Ninja generators
    SET(MAKE_COMMAND make)
  ENDIF()
  
  ExternalProject_Add(jemalloc
    PREFIX extra/jemalloc
    SOURCE_DIR ${SOURCE_DIR}
    BINARY_DIR ${BINARY_DIR}
    STAMP_DIR  ${BINARY_DIR}
    CONFIGURE_COMMAND "${SOURCE_DIR}/configure" ${JEMALLOC_CONFIGURE_OPTS}
    BUILD_COMMAND  ${MAKE_COMMAND} "build_lib_static"
    INSTALL_COMMAND ""
  )
  ADD_LIBRARY(libjemalloc STATIC IMPORTED)
  SET_TARGET_PROPERTIES(libjemalloc PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/lib/libjemalloc_pic.a")
  ADD_DEPENDENCIES(libjemalloc jemalloc)
ENDMACRO()

IF(CMAKE_SYSTEM_NAME MATCHES "Linux" OR APPLE)
 # Linux and OSX are the only systems where bundled jemalloc can be built without problems, 
 # as they both have GNU make and jemalloc actually compiles.
 # Also, BSDs use jemalloc as malloc already
 SET(WITH_JEMALLOC_DEFAULT "yes")
ELSE()
 SET(WITH_JEMALLOC_DEFAULT "no")
ENDIF()

SET(WITH_JEMALLOC ${WITH_JEMALLOC_DEFAULT} CACHE STRING
    "Which jemalloc to use (possible values are 'no', 'bundled', 'system', 'yes' (system if possible, otherwise bundled)")

MACRO (CHECK_JEMALLOC)
  IF(WITH_JEMALLOC STREQUAL "system" OR WITH_JEMALLOC STREQUAL "yes")
    CHECK_LIBRARY_EXISTS(jemalloc malloc_stats_print "" HAVE_JEMALLOC)
    IF (HAVE_JEMALLOC)
      SET(LIBJEMALLOC jemalloc)
    ELSEIF (WITH_JEMALLOC STREQUAL "system")
      MESSAGE(FATAL_ERROR "system jemalloc is not found")
    ELSEIF (WITH_JEMALLOC STREQUAL "yes")
      SET(trybundled 1)
    ENDIF()
  ENDIF()
  IF(WITH_JEMALLOC STREQUAL "bundled" OR trybundled)
    USE_BUNDLED_JEMALLOC()
  ENDIF()
ENDMACRO()

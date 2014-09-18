INCLUDE (CheckLibraryExists)

SET(WITH_JEMALLOC auto CACHE STRING
  "Build with jemalloc (possible values are 'yes', 'no', 'auto')")

MACRO (CHECK_JEMALLOC)
  # compatibility with old WITH_JEMALLOC values
  IF(WITH_JEMALLOC STREQUAL "bundled")
    MESSAGE(FATAL_ERROR "MariaDB no longer bundles jemalloc")
  ENDIF()
  IF(WITH_JEMALLOC STREQUAL "system")
    SET(WITH_JEMALLOC "yes")
  ENDIF()

  IF(WITH_JEMALLOC STREQUAL "yes" OR WITH_JEMALLOC STREQUAL "auto" OR
      WITH_JEMALLOC STREQUAL "static")

    IF(WITH_JEMALLOC STREQUAL "static")
      SET(libname jemalloc_pic)
      SET(CMAKE_REQUIRED_LIBRARIES pthread dl m)
    ELSE()
      SET(libname jemalloc)
    ENDIF()

    CHECK_LIBRARY_EXISTS(${libname} malloc_stats_print "" HAVE_JEMALLOC)
    SET(CMAKE_REQUIRED_LIBRARIES)

    IF (HAVE_JEMALLOC)
      SET(LIBJEMALLOC ${libname})
    ELSEIF (NOT WITH_JEMALLOC STREQUAL "auto")
      MESSAGE(FATAL_ERROR "${libname} is not found")
    ENDIF()
  ENDIF()
ENDMACRO()

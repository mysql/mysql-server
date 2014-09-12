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

  IF(WITH_JEMALLOC STREQUAL "yes" OR WITH_JEMALLOC STREQUAL "auto")
    CHECK_LIBRARY_EXISTS(jemalloc malloc_stats_print "" HAVE_JEMALLOC)
    IF (HAVE_JEMALLOC)
      SET(LIBJEMALLOC jemalloc)
    ELSEIF (WITH_JEMALLOC STREQUAL "yes")
      MESSAGE(FATAL_ERROR "jemalloc is not found")
    ENDIF()
  ENDIF()
ENDMACRO()

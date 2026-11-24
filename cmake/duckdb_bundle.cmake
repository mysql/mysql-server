# Copyright (c) 2026, Alibaba and/or its affiliates. All Rights Reserved.
# This program is free software under the GNU General Public License, version 2.0.

FILE(GLOB_RECURSE DUCKDB_ARCHIVES "${DUCKDB_BINARY_DIR}/*.a")
LIST(REMOVE_ITEM DUCKDB_ARCHIVES "${DUCKDB_BUNDLE}")
# The dummy loader is for standalone loadable extensions. The server needs
# the generated loader, which registers all bundled static extensions.
LIST(FILTER DUCKDB_ARCHIVES EXCLUDE REGEX
  "/libdummy_static_extension_loader\\.a$")
IF(NOT DUCKDB_ARCHIVES)
  MESSAGE(FATAL_ERROR "No DuckDB static libraries in ${DUCKDB_BINARY_DIR}")
ENDIF()

SET(REBUILD_BUNDLE OFF)
IF(NOT EXISTS "${DUCKDB_BUNDLE}")
  SET(REBUILD_BUNDLE ON)
ENDIF()
FOREACH(ARCHIVE ${DUCKDB_ARCHIVES})
  IF("${ARCHIVE}" IS_NEWER_THAN "${DUCKDB_BUNDLE}")
    SET(REBUILD_BUNDLE ON)
  ENDIF()
ENDFOREACH()
IF(NOT REBUILD_BUNDLE)
  RETURN()
ENDIF()

SET(MRI "CREATE ${DUCKDB_BUNDLE}\n")
FOREACH(ARCHIVE ${DUCKDB_ARCHIVES})
  STRING(APPEND MRI "ADDLIB ${ARCHIVE}\n")
ENDFOREACH()
STRING(APPEND MRI "SAVE\nEND\n")
FILE(WRITE "${DUCKDB_BINARY_DIR}/bundle.mri" "${MRI}")
EXECUTE_PROCESS(COMMAND "${DUCKDB_AR}" -M
  INPUT_FILE "${DUCKDB_BINARY_DIR}/bundle.mri"
  RESULT_VARIABLE RESULT)
IF(NOT RESULT EQUAL 0)
  MESSAGE(FATAL_ERROR "Failed to bundle DuckDB static libraries: ${RESULT}")
ENDIF()

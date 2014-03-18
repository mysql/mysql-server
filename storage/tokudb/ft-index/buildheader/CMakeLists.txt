set_directory_properties(PROPERTIES INCLUDE_DIRECTORIES "")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/runcat.sh" "#!/bin/bash
out=$1; shift
exec \"$@\" >$out")

add_executable(make_tdb make_tdb.cc)
set_property(TARGET make_tdb APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/db.h"
  COMMAND bash runcat.sh "${CMAKE_CURRENT_BINARY_DIR}/db.h" $<TARGET_FILE:make_tdb>
  DEPENDS make_tdb)
add_custom_target(install_tdb_h DEPENDS
  "${CMAKE_CURRENT_BINARY_DIR}/db.h")

# detect when we are being built as a subproject
if (NOT DEFINED MYSQL_PROJECT_NAME_DOCSTRING)
  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/db.h"
    DESTINATION include
    RENAME tokudb.h
    COMPONENT tokukv_headers
    )
  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/db.h"
    DESTINATION include
    COMPONENT tokukv_headers
    )
endif ()

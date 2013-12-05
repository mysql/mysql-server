# This configuration file builds both the static and shared version of
# the library.
set(replication_sources
  field_iterator.cpp row_of_fields.cpp rows_event.cpp
  binary_log.cpp protocol.cpp value.cpp binlog_event.cpp
  utilities.cpp)

# Configure for building static library
add_library(binlogapi_static STATIC ${replication_sources})

if (HAVE_MYSYS)
target_link_libraries(binlogapi_static ${ZLIB_LIBRARIES} mysys dbug)
else (HAVE_MYSYS)
target_link_libraries(binlogapi_static ${ZLIB_LIBRARIES})
endif(HAVE_MYSYS)

set_target_properties(binlogapi_static PROPERTIES
  OUTPUT_NAME "binlogapi")

install(TARGETS binlogapi_static
        ARCHIVE DESTINATION libbinlogapi/lib)

# Configure for building shared library
if (NOT HAVE_MYSYS)
add_library(binlogapi_shared SHARED ${replication_sources})
target_link_libraries(binlogapi_shared ${ZLIB_LIBRARIES})
set_target_properties(binlogapi_shared PROPERTIES
                      OUTPUT_NAME "binlogapi")

install(TARGETS binlogapi_shared
        LIBRARY DESTINATION libbinlogapi/lib
        ARCHIVE DESTINATION ${PROJECT_BINARY_DIR}/libbinlogapi/lib)

endif (NOT HAVE_MYSYS)


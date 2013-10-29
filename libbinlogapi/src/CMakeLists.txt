# This configuration file builds both the static and shared version of
# the library.
set(replication_sources
  field_iterator.cpp row_of_fields.cpp
  binary_log.cpp protocol.cpp value.cpp binlog_event.cpp
  bapi_utility.cpp utilities.cpp)

# Configure for building static library
add_library(binlogapi_static STATIC ${replication_sources})
set_target_properties(binlogapi_static PROPERTIES
  OUTPUT_NAME "binlogapi")

# Configure for building shared library
add_library(binlogapi_shared SHARED ${replication_sources})
set_target_properties(binlogapi_shared PROPERTIES
 OUTPUT_NAME "binlogapi")

install(TARGETS binlogapi_shared LIBRARY
        DESTINATION libbinlogapi/lib
        ARCHIVE libbinlogapi/lib)
install(TARGETS binlogapi_static ARCHIVE DESTINATION libbinlogapi/lib)

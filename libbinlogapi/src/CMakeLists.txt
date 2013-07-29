# This configuration file builds both the static and shared version of
# the library.
set(replication_sources
  access_method_factory.cpp field_iterator.cpp row_of_fields.cpp
  binlog_driver.cpp basic_transaction_parser.cpp tcp_driver.cpp
  file_driver.cpp binary_log.cpp protocol.cpp value.cpp binlog_event.cpp
  resultset_iterator.cpp basic_transaction_parser.cpp
  basic_content_handler.cpp utilities.cpp)

# Configure for building static library
add_library(replication_static STATIC ${replication_sources})
target_link_libraries(replication_static crypto ${Boost_LIBRARIES})
set_target_properties(replication_static PROPERTIES
  OUTPUT_NAME "replication")

# Configure for building shared library
add_library(replication_shared SHARED ${replication_sources})
target_link_libraries(replication_shared crypto ${Boost_LIBRARIES})

set_target_properties(replication_shared PROPERTIES
  VERSION 0.1 SOVERSION 1
  OUTPUT_NAME "replication")

install(TARGETS replication_shared LIBRARY DESTINATION lib)
install(TARGETS replication_static ARCHIVE DESTINATION lib)

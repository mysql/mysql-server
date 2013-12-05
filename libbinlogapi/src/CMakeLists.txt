# This configuration file builds both the static and shared version of
# the library.
SET (REPLICATION_SOURCES
     field_iterator.cpp
     row_of_fields.cpp
     rows_event.cpp
     binary_log.cpp
     protocol.cpp
     value.cpp
     binlog_event.cpp
     bapi_utility.cpp
     utilities.cpp)


# Configure for building static library
ADD_LIBRARY (binlogapi_static STATIC ${REPLICATION_SOURCES})

IF (HAVE_MYSYS)
  TARGET_LINK_LIBRARIES (binlogapi_static ${ZLIB_LIBRARIES} mysys dbug)
ELSE (HAVE_MYSYS)
  TARGET_LINK_LIBRARIES (binlogapi_static ${ZLIB_LIBRARIES})
ENDIF (HAVE_MYSYS)

SET_TARGET_PROPERTIES (binlogapi_static PROPERTIES
                       OUTPUT_NAME "binlogapi")


# Configure for building shared library
IF (NOT HAVE_MYSYS)
  ADD_LIBRARY (binlogapi_shared SHARED ${REPLICATION_SOURCES})
  TARGET_LINK_LIBRARIES (binlogapi_shared ${ZLIB_LIBRARIES})
  SET_TARGET_PROPERTIES (binlogapi_shared PROPERTIES
                         OUTPUT_NAME "binlogapi")
  INSTALL (TARGETS binlogapi_shared
           LIBRARY DESTINATION lib)
ENDIF (NOT HAVE_MYSYS)

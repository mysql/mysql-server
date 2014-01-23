# This configuration file builds both the static and shared version of
# the library.
SET (REPLICATION_SOURCES
     field_iterator.cpp
     load_data_events.cpp
     row_of_fields.cpp
     rows_event.cpp
     value.cpp
     binlog_event.cpp
    )


# Configure for building static library
ADD_LIBRARY (binlogevent_static STATIC ${REPLICATION_SOURCES})

IF (HAVE_MYSYS)
  TARGET_LINK_LIBRARIES (binlogevent_static ${ZLIB_LIBRARIES} mysys dbug)
ELSE (HAVE_MYSYS)
  TARGET_LINK_LIBRARIES (binlogevent_static ${ZLIB_LIBRARIES})
ENDIF (HAVE_MYSYS)

SET_TARGET_PROPERTIES (binlogevent_static PROPERTIES
                       OUTPUT_NAME "binlogevent")


# Configure for building shared library
IF (NOT HAVE_MYSYS)
  ADD_LIBRARY (binlogevent_shared SHARED ${REPLICATION_SOURCES})
  TARGET_LINK_LIBRARIES (binlogevent_shared ${ZLIB_LIBRARIES})
  SET_TARGET_PROPERTIES (binlogevent_shared PROPERTIES
                         OUTPUT_NAME "binlogevent")
  INSTALL (TARGETS binlogevent_shared
           LIBRARY DESTINATION lib)
ENDIF (NOT HAVE_MYSYS)

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
ADD_LIBRARY (binlogevents_static STATIC ${REPLICATION_SOURCES})

IF (HAVE_MYSYS)
  TARGET_LINK_LIBRARIES (binlogevents_static ${ZLIB_LIBRARIES} mysys dbug)
ELSE (HAVE_MYSYS)
  TARGET_LINK_LIBRARIES (binlogevents_static ${ZLIB_LIBRARIES})
ENDIF (HAVE_MYSYS)

SET_TARGET_PROPERTIES (binlogevents_static PROPERTIES
                       OUTPUT_NAME "binlogevents")


# Configure for building shared library
IF (NOT HAVE_MYSYS)
  ADD_LIBRARY (binlogevents_shared SHARED ${REPLICATION_SOURCES})
  TARGET_LINK_LIBRARIES (binlogevents_shared ${ZLIB_LIBRARIES})
  SET_TARGET_PROPERTIES (binlogevents_shared PROPERTIES
                         OUTPUT_NAME "binlogevents")
  INSTALL (TARGETS binlogevents_shared
           LIBRARY DESTINATION lib)
ENDIF (NOT HAVE_MYSYS)

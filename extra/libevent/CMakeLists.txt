INCLUDE_DIRECTORIES(
	${CMAKE_SOURCE_DIR}/extra/libevent
	${CMAKE_SOURCE_DIR}/extra/libevent/compat
	${CMAKE_SOURCE_DIR}/extra/libevent/WIN32-Code
	${CMAKE_BINARY_DIR}/extra/libevent
	${CMAKE_SOURCE_DIR}/include
)
	
IF(MSVC)
	ADD_DEFINITIONS("-DWIN32 -DHAVE_CONFIG_H")
ENDIF(MSVC)

SET(LIBEVENT_SOURCES
	buffer.c 
	evbuffer.c
	event.c
	evutil.c
	log.c
	signal.c
	strlcpy.c
	WIN32-Code/win32.c
	WIN32-Code/config.h
	WIN32-Code/misc.c
	WIN32-Code/misc.h
	event-internal.h
	event.h
	evsignal.h
	evutil.h
	log.h
	min_heap.h
	strlcpy-internal.h
	)
IF(WIN32)
  # Workaround source distribution bug, remove preconfigured event-config
  IF(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    FILE(REMOVE ${CMAKE_SOURCE_DIR}/extra/libevent/event-config.h)
  ENDIF()
  CONFIGURE_FILE(WIN32-Code/config.h ${CMAKE_BINARY_DIR}/extra/libevent/event-config.h COPYONLY)
ENDIF()

IF(NOT SOURCE_SUBLIBS)
	ADD_LIBRARY(libevent ${LIBEVENT_SOURCES})
ENDIF(NOT SOURCE_SUBLIBS)

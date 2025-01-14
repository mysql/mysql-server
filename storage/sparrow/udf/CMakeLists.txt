SET(SPARROWUDF_SOURCES
	udf.h
	udf.cc
	operator.h
	operator.cc
	udfargument.h
	udfargument.cc)

ADD_DEFINITIONS(-DMYSQL_SERVER -DHAVE_DLOPEN)
ADD_LIBRARY(sparrowudf MODULE ${SPARROWUDF_SOURCES})

# sparrowudf depends on strings
IF(WIN32)
  IF(MSVC)
    SET_TARGET_PROPERTIES(sparrowudf PROPERTIES LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/udf.def")
  ENDIF()
  TARGET_LINK_LIBRARIES(sparrowudf strings)
ENDIF()

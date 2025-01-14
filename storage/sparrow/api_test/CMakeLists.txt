SET(SPARROW_API_TEST_SOURCES
	exception.cc
	exception.h
	sparrow_api_test.cpp
	all_types.h
	all_types.cpp
	column_subset.h
	column_subset.cpp
	many_partitions.h
	many_partitions.cpp
	too_many_columns.h
	too_many_columns.cpp
	vl.h
	vl.cpp
	column_optim.h
	column_optim.cpp
	sql.h
	sql.cpp
	utils.h
	common.h
	common.cpp
	errors.h
	errors.cpp)

	
REMOVE_DEFINITIONS(-DSPARROW_API_EXPORTS)

MYSQL_ADD_EXECUTABLE(sparrow_api_test ${SPARROW_API_TEST_SOURCES}
	LINK_LIBRARIES sparrowapi)

TARGET_INCLUDE_DIRECTORIES(sparrow_api_test PRIVATE ${CMAKE_SOURCE_DIR}/storage/sparrow/api/include)

# Actually, sparrowapi embeds mysqlclient, but it's clearer if we specify explicitly the mysqlclient. 
TARGET_LINK_LIBRARIES(sparrow_api_test sparrowapi mysqlclient)


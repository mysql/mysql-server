INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/libmysqld/include
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include)

# Currently does not work with DBUG, there are missing symbols reported.
ADD_DEFINITIONS(-DDBUG_OFF)
ADD_EXECUTABLE(test_libmysqld ../../client/completion_hash.cc
               ../../client/mysql.cc ../../client/readline.cc
               ../../client/sql_string.cc)
TARGET_LINK_LIBRARIES(test_libmysqld yassl taocrypt zlib wsock32)
ADD_DEPENDENCIES(test_libmysqld libmysqld)

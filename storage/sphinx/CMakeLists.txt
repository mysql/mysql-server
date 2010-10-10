INCLUDE("${PROJECT_SOURCE_DIR}/storage/mysql_storage_engine.cmake")

ADD_DEFINITIONS(-DMYSQL_SERVER)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include
                    ${CMAKE_SOURCE_DIR}/regex)

SET(SPHINX_SOURCES ha_sphinx.cc)
SET(SPHINX_LIBS ws2_32.lib)
MYSQL_STORAGE_ENGINE(SPHINX)

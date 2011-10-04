INCLUDE("${PROJECT_SOURCE_DIR}/storage/mysql_storage_engine.cmake")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/sql ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include)

SET(FEEDBACK_SOURCES feedback.cc sender_thread.cc
    url_base.cc url_http.cc utils.cc)

SET(FEEDBACK_LIBS Ws2_32)

MYSQL_PLUGIN(FEEDBACK)

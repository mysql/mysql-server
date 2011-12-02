INCLUDE("${PROJECT_SOURCE_DIR}/storage/mysql_storage_engine.cmake")
SET(AUTH_SOURCES dialog.c ${CMAKE_SOURCE_DIR}/libmysql/get_password.c)
MYSQL_PLUGIN(AUTH)

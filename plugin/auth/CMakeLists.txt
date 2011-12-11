INCLUDE("${PROJECT_SOURCE_DIR}/storage/mysql_storage_engine.cmake")
SET(AUTH_SOURCES dialog.c)
MYSQL_PLUGIN(AUTH)

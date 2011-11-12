INCLUDE("${PROJECT_SOURCE_DIR}/storage/mysql_storage_engine.cmake")
SET(FTEXAMPLE_SOURCES plugin_example.c)
MYSQL_PLUGIN(FTEXAMPLE)

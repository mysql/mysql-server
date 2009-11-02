INCLUDE("${PROJECT_SOURCE_DIR}/storage/mysql_storage_engine.cmake")
SET(FEDERATEDX_SOURCES  ha_federatedx.cc)
MYSQL_STORAGE_ENGINE(FEDERATEDX)

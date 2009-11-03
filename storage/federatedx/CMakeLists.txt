INCLUDE("${PROJECT_SOURCE_DIR}/storage/mysql_storage_engine.cmake")
SET(FEDERATEDX_SOURCES  ha_federatedx.cc federatedx_txn.cc federatedx_io.cc federatedx_io_null.cc federatedx_io_mysql.cc)
MYSQL_STORAGE_ENGINE(FEDERATEDX)

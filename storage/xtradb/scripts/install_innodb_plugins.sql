-- execute these to install InnoDB if it is built as a dynamic plugin
INSTALL PLUGIN innodb SONAME 'ha_innodb.so';
INSTALL PLUGIN innodb_trx SONAME 'ha_innodb.so';
INSTALL PLUGIN innodb_locks SONAME 'ha_innodb.so';
INSTALL PLUGIN innodb_lock_waits SONAME 'ha_innodb.so';
INSTALL PLUGIN innodb_cmp SONAME 'ha_innodb.so';
INSTALL PLUGIN innodb_cmp_reset SONAME 'ha_innodb.so';
INSTALL PLUGIN innodb_cmpmem SONAME 'ha_innodb.so';
INSTALL PLUGIN innodb_cmpmem_reset SONAME 'ha_innodb.so';
INSTALL PLUGIN XTRADB_ENHANCEMENTS SONAME 'ha_innodb.so';
INSTALL PLUGIN INNODB_BUFFER_POOL_PAGES SONAME 'ha_innodb.so';
INSTALL PLUGIN INNODB_BUFFER_POOL_PAGES_BLOB SONAME 'ha_innodb.so';
INSTALL PLUGIN INNODB_BUFFER_POOL_PAGES_INDEX SONAME 'ha_innodb.so';
INSTALL PLUGIN innodb_rseg SONAME 'ha_innodb.so';
INSTALL PLUGIN innodb_table_stats SONAME 'ha_innodb.so';
INSTALL PLUGIN innodb_index_stats SONAME 'ha_innodb.so';

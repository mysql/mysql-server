const char *load_default_groups[]= {
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
"mysql_cluster",
#endif
"mysqld", "server", MYSQL_BASE_VERSION,
"mariadb", MARIADB_BASE_VERSION,
"client-server",
0, 0};

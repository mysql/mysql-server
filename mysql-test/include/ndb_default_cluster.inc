-- require r/ndb_default_cluster.require
disable_query_log;
show status like "Ndb_connected_host";
enable_query_log;

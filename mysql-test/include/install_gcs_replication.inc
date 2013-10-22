--let $include_filename= install_gcs_replication.inc
--source include/begin_include_file.inc

# Synchronous replication will work only for RBR mode of replication thus tests
# using it are restricted to this mode only.

--source include/have_binlog_format_row.inc
--source include/not_embedded.inc
--source include/have_innodb.inc
--source include/have_gtid.inc

--disable_query_log
--eval SET sql_log_bin = 0;
--replace_regex /\.dll/.so/
--eval INSTALL PLUGIN gcs_replication_plugin SONAME '$GCS_RPL_PLUGIN'
--eval SET sql_log_bin = 1;
--enable_query_log
--source include/end_include_file.inc

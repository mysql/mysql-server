# Create and drop a table, so that purge has something to do.
--disable_query_log
create table tmp_purge (a int primary key) engine innodb STATS_PERSISTENT=0;
drop table tmp_purge;
SET GLOBAL innodb_purge_run_now=ON;
--enable_query_log
# run purge and wait until done.
--source include/wait_innodb_all_purged.inc

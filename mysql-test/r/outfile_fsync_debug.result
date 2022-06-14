# restart
SET SESSION select_into_buffer_size=262144;
CREATE TABLE t1 (a TEXT, b TEXT) ENGINE=INNODB;
CREATE TABLE t2 (a TEXT, b TEXT) ENGINE=INNODB;
include/diff_tables.inc [test.t1, test.t2]
ERROR HY000: Query execution was interrupted, maximum statement execution time exceeded
include/assert_grep.inc [Found expected number of select_to_file]
include/assert_grep.inc [Found expected number of select_to_file]
include/assert_grep.inc [Found expected number of select_to_file]
SET GLOBAL debug = '-d,print_select_into_flush_stats';
SELECT /*+ SET_VAR(select_into_disk_sync_delay = 5000) 
SET_VAR(select_into_disk_sync = ON)
SET_VAR(select_into_buffer_size = 16384) */
@@select_into_disk_sync_delay, @@select_into_disk_sync, @@select_into_buffer_size;
@@select_into_disk_sync_delay	@@select_into_disk_sync	@@select_into_buffer_size
5000	1	16384
SET SESSION select_into_buffer_size=default;
DROP TABLE t1, t2;

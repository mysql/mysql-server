#
# Normal Case
#
set global innodb_limit_optimistic_insert_debug = 2;
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (10), (20), (30), (40), (50), (60), (70), (80), (90), (100);
SELECT COUNT(*) FROM t1;
COUNT(*)
10
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DROP TABLE t1;
set global innodb_limit_optimistic_insert_debug = 0;
#
# Error test case where we run out of resources while spawning threads and the worker threads exit
#
call mtr.add_suppression("\\[Warning\\] .* Resource not available to create threads for parallel scan. Falling back to single thread mode.");
set global innodb_limit_optimistic_insert_debug = 2;
set debug="+d,innodb_pread_thread_OOR";
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (10), (20), (30), (40), (50), (60), (70), (80), (90), (100);
SELECT COUNT(*) FROM t1;
COUNT(*)
10
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DROP TABLE t1;
set global innodb_limit_optimistic_insert_debug = 0;
set debug="-d,innodb_pread_thread_OOR";

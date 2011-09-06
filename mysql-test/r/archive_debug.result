#
# BUG#12402794 - 60976: CRASH, VALGRIND WARNING AND MEMORY LEAK
#                       WITH PARTITIONED ARCHIVE TABLES
#
CREATE TABLE t1(a INT) ENGINE=ARCHIVE;
INSERT INTO t1 VALUES(1);
SET SESSION debug='d,simulate_archive_open_failure';
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	error	Corrupt
SET SESSION debug=DEFAULT;
DROP TABLE t1;

#
# BUG#41330 - Myisam table open count set to zero before index blocks are written.
#
# Don't test this under valgrind, memory leaks will occur
# Binary must be compiled with debug for crash to occur
call mtr.add_suppression("Got an error from thread_id=.*ha_myisam.cc:");
call mtr.add_suppression("MySQL thread id .*, query id .* localhost.*root Checking table");
SET GLOBAL delay_key_write=ALL;
CREATE TABLE t1(a INT, 
b INT, 
PRIMARY KEY(a , b), 
KEY(b)) ENGINE=MyISAM DELAY_KEY_WRITE = 1;
INSERT INTO t1 VALUES (1,2),(2,3),(3,4),(4,5),(5,6);
# Setup the mysqld to crash at certain point
SET SESSION debug="d,crash_before_flush_keys";
# Write file to make mysql-test-run.pl expect crash
# Run the crashing query
FLUSH TABLE t1;
ERROR HY000: Lost connection to MySQL server during query
# Write file to make mysql-test-run.pl start the server 
# Turn on reconnect
# Call script that will poll the server waiting for
# it to be back online again
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	warning	1 client is using or hasn't closed the table properly
test.t1	check	error	Size of indexfile is: 1024        Should be: 3072
test.t1	check	error	Corrupt
DROP TABLE t1;

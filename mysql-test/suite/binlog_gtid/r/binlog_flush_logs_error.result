call mtr.add_suppression('Could not open .*');
call mtr.add_suppression("Either disk is full or file system is read only");
call mtr.add_suppression("An out-of-memory error occurred while saving the set of GTID.*");
call mtr.add_suppression("Attempting backtrace. You can use the following information to find out");
call mtr.add_suppression("Got error .*");
CALL mtr.add_suppression("Unable to create a new binlog file.");
FLUSH LOGS;
SET GLOBAL debug = '+d,simulate_flush_commit_error';
CREATE TABLE t(s INT);
INSERT INTO t VALUES(10);
FLUSH LOGS;
Got one of the listed errors
SET GLOBAL debug = '-d,simulate_flush_commit_error';
DROP TABLE t;

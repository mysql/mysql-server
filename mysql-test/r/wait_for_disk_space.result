CALL mtr.add_suppression('Disk is full writing');
CALL mtr.add_suppression('Retry in 60 secs');
CREATE TABLE t1 (c1 INT, c2 LONGTEXT) ENGINE=MyISAM;
INSERT INTO t1 VALUES (1, REPEAT("a", 8192));
INSERT INTO t1 VALUES (2, REPEAT("b", 8192));
INSERT INTO t1 VALUES (3, REPEAT("c", 8192));
# Adding debug point 'force_wait_for_disk_space' to @@GLOBAL.debug
REPAIR TABLE t1;
include/assert.inc [REPAIR TABLE PROCESSLIST_STATE must not be "Waiting for disk space"]
# Removing debug point 'force_wait_for_disk_space' from @@GLOBAL.debug
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
DROP TABLE t1;

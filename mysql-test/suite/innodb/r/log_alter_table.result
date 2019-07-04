#
# Bug#21801423 INNODB REDO LOG DOES NOT INDICATE WHEN
# FILES ARE CREATED
#
# Bug#21796691 INNODB REDO LOG DOES NOT INDICATE WHEN
# REDO LOGGING IS SKIPPED
#
SET GLOBAL innodb_checkpoint_disabled=1;
CREATE TABLE t1 (a INT NOT NULL, b INT UNIQUE) ENGINE=InnoDB TABLESPACE=innodb_file_per_table;
INSERT INTO t1 VALUES (1,2);
ALTER TABLE t1 ADD PRIMARY KEY(a), ALGORITHM=INPLACE;
ALTER TABLE t1 DROP INDEX b, ADD INDEX (b);
# Kill the server
# restart: --debug=d,ib_log
Pattern "scan .*:.*log rec MLOG_FILE_CREATE.*page .*:0" found
Pattern "scan .*:.*log rec MLOG_INDEX_LOAD" found
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
# restart;
DROP TABLE t1;

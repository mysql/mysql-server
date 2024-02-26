SET @@GLOBAL.default_tmp_storage_engine = MyISAM;
SET @@session.default_tmp_storage_engine = MyISAM;
SELECT @@global.default_tmp_storage_engine;
@@global.default_tmp_storage_engine
MyISAM
SELECT @@session.default_tmp_storage_engine;
@@session.default_tmp_storage_engine
MyISAM
CREATE TEMPORARY TABLE t1(a INT);
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
REPAIR TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
DROP TABLE t1;
SET @@global.default_tmp_storage_engine = MyISAM;
SET @@session.default_tmp_storage_engine = MyISAM;
SELECT @@global.default_tmp_storage_engine;
@@global.default_tmp_storage_engine
MyISAM
SELECT @@session.default_tmp_storage_engine;
@@session.default_tmp_storage_engine
MyISAM
CREATE TEMPORARY TABLE t1(a INT);
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
REPAIR TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
DROP TABLE t1;
# Checking MyISAM storage engine
CREATE TABLE t2(i INT) ENGINE=MYISAM;
# Table has been created with last_checked_for_upgrade = 0,
# so the check will be performed
CHECK TABLE t2 FOR UPGRADE;
Table	Op	Msg_type	Msg_text
test.t2	check	status	OK
# Checking again will detect that the table has already been checked
CHECK TABLE t2 FOR UPGRADE;
Table	Op	Msg_type	Msg_text
test.t2	check	status	Table is already up to date
ALTER TABLE t2 CHANGE COLUMN i j INT, ALGORITHM=INPLACE;
# Checking again after ALTER INPLACE to verify that the check status is
# still present
CHECK TABLE t2 FOR UPGRADE;
Table	Op	Msg_type	Msg_text
test.t2	check	status	Table is already up to date
ALTER TABLE t2 CHANGE COLUMN j k INT, ALGORITHM=COPY;
# Checking again after ALTER COPY to verify that the check status is
# still present
CHECK TABLE t2 FOR UPGRADE;
Table	Op	Msg_type	Msg_text
test.t2	check	status	Table is already up to date
TRUNCATE TABLE t2;
# Checking again after TRUNCATE to verify that the check status is
# still present
CHECK TABLE t2 FOR UPGRADE;
Table	Op	Msg_type	Msg_text
test.t2	check	status	Table is already up to date
# Cleanup
DROP TABLE t2;

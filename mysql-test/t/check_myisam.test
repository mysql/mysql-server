--source include/force_myisam_default.inc
--source include/have_myisam.inc


#
# Bug#26325 TEMPORARY TABLE "corrupt" after first read, according to CHECK TABLE
#

# Repair table is supported only by MyISAM engine

let $global_tmp_storage_engine  = `select @@global.default_tmp_storage_engine`;
let $session_tmp_storage_engine = `select @@session.default_tmp_storage_engine`;

SET @@GLOBAL.default_tmp_storage_engine = MyISAM;
SET @@session.default_tmp_storage_engine = MyISAM;

SELECT @@global.default_tmp_storage_engine;
SELECT @@session.default_tmp_storage_engine;

CREATE TEMPORARY TABLE t1(a INT);
CHECK TABLE t1;
REPAIR TABLE t1;
DROP TABLE t1;

# Reset the variables to MTR default
eval SET @@global.default_tmp_storage_engine = $global_tmp_storage_engine;
eval SET @@session.default_tmp_storage_engine = $session_tmp_storage_engine;

SELECT @@global.default_tmp_storage_engine;
SELECT @@session.default_tmp_storage_engine;

CREATE TEMPORARY TABLE t1(a INT);
CHECK TABLE t1;
REPAIR TABLE t1;
DROP TABLE t1;

--echo # Checking MyISAM storage engine
CREATE TABLE t2(i INT) ENGINE=MYISAM;

--echo # Table has been created with last_checked_for_upgrade = 0,
--echo # so the check will be performed
CHECK TABLE t2 FOR UPGRADE;

--echo # Checking again will detect that the table has already been checked
CHECK TABLE t2 FOR UPGRADE;

ALTER TABLE t2 CHANGE COLUMN i j INT, ALGORITHM=INPLACE;

--echo # Checking again after ALTER INPLACE to verify that the check status is
--echo # still present
CHECK TABLE t2 FOR UPGRADE;

ALTER TABLE t2 CHANGE COLUMN j k INT, ALGORITHM=COPY;

--echo # Checking again after ALTER COPY to verify that the check status is
--echo # still present
CHECK TABLE t2 FOR UPGRADE;

TRUNCATE TABLE t2;

--echo # Checking again after TRUNCATE to verify that the check status is
--echo # still present
CHECK TABLE t2 FOR UPGRADE;

--echo # Cleanup
DROP TABLE t2;

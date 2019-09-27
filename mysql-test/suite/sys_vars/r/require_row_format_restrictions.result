#
# Step 0. Create some tables for testing
CREATE TABLE t1 (c1 INT) Engine=InnoDB;
INSERT INTO t1 VALUES (1);
CREATE TABLE t2 (c1 INT) Engine=InnoDB;
INSERT INTO t2 VALUES (10);
CREATE TABLE t3 (text VARCHAR(20));
CREATE TEMPORARY TABLE temp1 (c1 INT);
#
# 1. Set require_row_format to 1 and check all restrictions are enforced.
SET SESSION require_row_format= 1;
INSERT INTO t1 VALUES (2);
ERROR HY000: The query does not comply with variable require_row_format restrictions.
UPDATE t1 SET c1 = 2 WHERE c1 = 1;
ERROR HY000: The query does not comply with variable require_row_format restrictions.
INSERT INTO t1 SELECT * FROM t2;
ERROR HY000: The query does not comply with variable require_row_format restrictions.
DELETE FROM t1 WHERE c1 = 1;
ERROR HY000: The query does not comply with variable require_row_format restrictions.
REPLACE INTO t1 (c1) VALUES(2);
ERROR HY000: The query does not comply with variable require_row_format restrictions.
REPLACE t1 SELECT * FROM t2;
ERROR HY000: The query does not comply with variable require_row_format restrictions.
LOAD DATA INFILE '../std_data_ln/words.dat' INTO TABLE t3;
ERROR HY000: The query does not comply with variable require_row_format restrictions.
DELETE t1, t2 FROM t1, t2 WHERE t1.c1 = 1;
ERROR HY000: The query does not comply with variable require_row_format restrictions.
UPDATE t1, t2 SET t1.c1=t2.c1 WHERE t1.c1 = 1;
ERROR HY000: The query does not comply with variable require_row_format restrictions.
CREATE TEMPORARY TABLE temp2 (c1 INT);
ERROR HY000: The query does not comply with variable require_row_format restrictions.
DROP TEMPORARY TABLE temp1;
ERROR HY000: The query does not comply with variable require_row_format restrictions.
include/assert.inc [Table t1 was not changed]
include/assert.inc [Table t3 was empty]
#
# 1.1 Test binlog updates can still update tables
BINLOG '
mV5DXQ8BAAAAeAAAAHwAAAABAAQAOC4wLjE5LXRyLWRlYnVnAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAACZXkNdEwANAAgAAAAABAAEAAAAYAAEGggAAAAICAgCAAAACgoKKioAEjQA
CgHG1zdG';
BINLOG '
mV5DXRMBAAAAMAAAADACAAAAAJoAAAAAAAEABHRlc3QAAnQxAAEDAAEBAQCnZmio
mV5DXR4BAAAAKAAAAFgCAAAAAJoAAAAAAAEAAgAB/wACAAAAntJyow==';
include/assert.inc [Table t1 was changed]
#
# 2. Set require_row_format to 0 and check all restrictions are gone
SET SESSION require_row_format= 0;
INSERT INTO t1 VALUES (11);
UPDATE t1 SET c1 = 4 WHERE c1 = 1;
CREATE TEMPORARY TABLE temp2 (c1 INT);
BINLOG '
mV5DXRMBAAAAMAAAAEEDAAAAAJoAAAAAAAEABHRlc3QAAnQxAAEDAAEBAQDNxAV8
mV5DXR4BAAAAKAAAAGkDAAAAAJoAAAAAAAEAAgAB/wADAAAAO7EClA==';
include/assert.inc [Table t1 has 4 values]
#
# 3. Cleanup
DROP TEMPORARY TABLE temp1;
DROP TEMPORARY TABLE temp2;
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;

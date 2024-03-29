SET GLOBAL innodb_compression_level = 0;
CREATE TABLESPACE ts1 FILE_BLOCK_SIZE=8K;
CREATE TABLE t1 (
id INT PRIMARY KEY,
make_big CHAR(200),
val LONGBLOB
) ROW_FORMAT=compressed TABLESPACE ts1;
CREATE TABLE t2 (
id INT PRIMARY KEY,
make_big CHAR(200),
val LONGBLOB
) ROW_FORMAT=compressed TABLESPACE ts1;
INSERT INTO t2 (id,val) VALUES (1,REPEAT('a',1000000));
INSERT INTO t1 (id,val) VALUES (2,REPEAT('a',1000000));
INSERT INTO t2 (id,val) VALUES (2,REPEAT('a',1000000));
INSERT INTO t1 (id,val) VALUES (3,REPEAT('a',1000000));
INSERT INTO t2 (id,val) VALUES (3,REPEAT('a',312000));
INSERT INTO t2 (id,val) VALUES (4,REPEAT('a',13));
BEGIN;
UPDATE t2 SET val = REPEAT('b',1000000) WHERE id=4;
SET DEBUG_SYNC=
'ib_blob_update_rollback_will_reserve
    SIGNAL con1_will_reserve
    WAIT_FOR con1_can_reserve';
ROLLBACK;
SET DEBUG_SYNC='now WAIT_FOR con1_will_reserve';
BEGIN;
INSERT INTO t1 (id,val) VALUES (4000,REPEAT('b',10000000));
SET DEBUG_SYNC='now SIGNAL con1_can_reserve';
ROLLBACK;
DROP TABLE t1;
DROP TABLE t2;
DROP TABLESPACE ts1;
SET GLOBAL innodb_compression_level = default;

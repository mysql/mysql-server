--echo #
--echo # Bug#30878065: EXCHANGING PARTITION WITH TABLES DOES NOT UPDATE SDI CORRECTLY
--echo #

--echo # Create partitioned table for testing exchange partition
CREATE TABLE pt1 (a INT NOT NULL, PRIMARY KEY(a))
    PARTITION BY RANGE (a) PARTITIONS 3 (
        PARTITION p1 VALUES LESS THAN (1000),
        PARTITION p2 VALUES LESS THAN (2000),
        PARTITION p3 VALUES LESS THAN (3000));

--echo # Create swap tables
CREATE TABLE t1 (a INT NOT NULL, PRIMARY KEY(a));
CREATE TABLE t2 (a INT NOT NULL, PRIMARY KEY(a));

--echo # Create src and dst schemata with tables for testing import tablespace
--echo # and import partition tablespace.
CREATE SCHEMA src;
USE src;
CREATE TABLE src_t1(c1 INT);
INSERT INTO src_t1 VALUES (0), (1), (2);

CREATE TABLE src_t2 (c1 INT)
PARTITION BY LIST (c1)
SUBPARTITION BY HASH (c1) SUBPARTITIONS 3
(PARTITION p0 VALUES IN (0, 2, 4, 6, 8),
 PARTITION p1 VALUES IN (1, 3, 5, 7, 9));

INSERT INTO src_t2 VALUES (1),(2),(3),(4),(5),(6),(7),(8),(9);
SELECT c1 FROM src_t2 ORDER BY c1;


CREATE SCHEMA dst;
USE dst;
CREATE TABLE dst_t1(c1 INT);

CREATE TABLE dst_t2 (c1 INT)
PARTITION BY LIST (c1)
SUBPARTITION BY HASH (c1) SUBPARTITIONS 3
(PARTITION p0 VALUES IN (1, 3, 5, 7, 9),
 PARTITION p1 VALUES IN (0, 2, 4, 6, 8));

INSERT INTO dst_t2 VALUES (1), (2), (3);
INSERT INTO dst_t2 VALUES (7), (8), (9);
SELECT c1 FROM dst_t2 ORDER BY c1;


let $MYSQLD_DATADIR = `select @@datadir`;

--echo # Shutdown the server to show SDI
--source include/shutdown_mysqld.inc

--echo # Dump SDI for swap table before exchange
replace_regex /version.*/VERSION/ /created.*/CREATED/
/last_altered.*/LAST_ALTERED/ /id.*/ID/ /\/\//\//;
--exec $IBD2SDI $MYSQLD_DATADIR/test/t1.ibd

--echo # Dump SDI for first partition's tablespace before exchange
replace_regex /version.*/VERSION/ /created.*/CREATED/
/last_altered.*/LAST_ALTERED/ /id.*/ID/ /\/\//\//;
--exec $IBD2SDI $MYSQLD_DATADIR/test/pt1#p#p1.ibd

--echo # Dump SDI for first subpartition's tablespace in source
--echo # table for import.
replace_regex /version.*/VERSION/ /created.*/CREATED/
/last_altered.*/LAST_ALTERED/ /id.*/ID/ /\/\//\//;
--exec $IBD2SDI $MYSQLD_DATADIR/src/src_t2#p#p0#sp#p0sp0.ibd

--echo # Restart server again
--source include/start_mysqld.inc

USE test;
--echo # Exchange with the first partition (which has SDIs)
ALTER TABLE pt1 EXCHANGE PARTITION p1 WITH TABLE t1;

--echo # Exchange with the second partitition (which does not have SDIs)
ALTER TABLE pt1 EXCHANGE PARTITION p2 with TABLE t2;

--echo # Copy snaphot of src_t1.ibd to tmp dir
USE src;

FLUSH TABLES src_t1 FOR EXPORT;

--copy_file $MYSQLD_DATADIR/src/src_t1.ibd $MYSQL_TMP_DIR/src_t1.ibd
--copy_file $MYSQLD_DATADIR/src/src_t1.cfg $MYSQL_TMP_DIR/src_t1.cfg

UNLOCK TABLES;

--echo # Copy snaphot of src_t2 ibd-s to tmp dir
FLUSH TABLES src_t2 FOR EXPORT;

--copy_file $MYSQLD_DATADIR/src/src_t2#p#p0#sp#p0sp0.ibd $MYSQL_TMP_DIR/src_t2#p#p0#sp#p0sp0.ibd
--copy_file $MYSQLD_DATADIR/src/src_t2#p#p0#sp#p0sp0.cfg $MYSQL_TMP_DIR/src_t2#p#p0#sp#p0sp0.cfg

--copy_file $MYSQLD_DATADIR/src/src_t2#p#p1#sp#p1sp0.ibd $MYSQL_TMP_DIR/src_t2#p#p1#sp#p1sp0.ibd
--copy_file $MYSQLD_DATADIR/src/src_t2#p#p1#sp#p1sp0.cfg $MYSQL_TMP_DIR/src_t2#p#p1#sp#p1sp0.cfg

--copy_file $MYSQLD_DATADIR/src/src_t2#p#p1#sp#p1sp1.ibd $MYSQL_TMP_DIR/src_t2#p#p1#sp#p1sp1.ibd
--copy_file $MYSQLD_DATADIR/src/src_t2#p#p1#sp#p1sp1.cfg $MYSQL_TMP_DIR/src_t2#p#p1#sp#p1sp1.cfg

--copy_file $MYSQLD_DATADIR/src/src_t2#p#p1#sp#p1sp2.ibd $MYSQL_TMP_DIR/src_t2#p#p1#sp#p1sp2.ibd
--copy_file $MYSQLD_DATADIR/src/src_t2#p#p1#sp#p1sp2.cfg $MYSQL_TMP_DIR/src_t2#p#p1#sp#p1sp2.cfg

UNLOCK TABLES;

--echo # Discard dst.dst_t1's tablespace and import src.src_t1's tablespace
--echo # its place.
USE dst;

LOCK TABLES dst_t1 WRITE;
ALTER TABLE dst_t1 DISCARD TABLESPACE;

--move_file $MYSQL_TMP_DIR/src_t1.ibd $MYSQLD_DATADIR/dst/dst_t1.ibd
--move_file $MYSQL_TMP_DIR/src_t1.cfg $MYSQLD_DATADIR/dst/dst_t1.cfg

ALTER TABLE dst_t1 IMPORT TABLESPACE;
UNLOCK TABLES;

LOCK TABLES dst_t2 WRITE;
ALTER TABLE dst_t2 DISCARD PARTITION p1sp0 TABLESPACE;

--echo # Copy the 1. subpartition of the first partition to the 1 subpartition of the second partition to
--echo # match the partitioning of the destination table, and to make sure that the partition tablespace
--echo # containing SDIs is NOT the first partition of the destination table.
--move_file $MYSQL_TMP_DIR/src_t2#p#p0#sp#p0sp0.ibd $MYSQLD_DATADIR/dst/dst_t2#p#p1#sp#p1sp0.ibd
--move_file $MYSQL_TMP_DIR/src_t2#p#p0#sp#p0sp0.cfg $MYSQLD_DATADIR/dst/dst_t2#p#p1#sp#p1sp0.cfg

ALTER TABLE dst_t2 IMPORT PARTITION p1sp0 TABLESPACE;
UNLOCK TABLES;

SELECT * FROM dst_t2 ORDER BY c1;

LOCK TABLES dst.dst_t2 WRITE;
ALTER TABLE dst_t2 DISCARD PARTITION p0 TABLESPACE;

--echo # Copy all of src p1 to dst p0
--move_file $MYSQL_TMP_DIR/src_t2#p#p1#sp#p1sp0.ibd $MYSQLD_DATADIR/dst/dst_t2#p#p0#sp#p0sp0.ibd
--move_file $MYSQL_TMP_DIR/src_t2#p#p1#sp#p1sp0.cfg $MYSQLD_DATADIR/dst/dst_t2#p#p0#sp#p0sp0.cfg

--move_file $MYSQL_TMP_DIR/src_t2#p#p1#sp#p1sp1.ibd $MYSQLD_DATADIR/dst/dst_t2#p#p0#sp#p0sp1.ibd
--move_file $MYSQL_TMP_DIR/src_t2#p#p1#sp#p1sp1.cfg $MYSQLD_DATADIR/dst/dst_t2#p#p0#sp#p0sp1.cfg

--move_file $MYSQL_TMP_DIR/src_t2#p#p1#sp#p1sp2.ibd $MYSQLD_DATADIR/dst/dst_t2#p#p0#sp#p0sp2.ibd
--move_file $MYSQL_TMP_DIR/src_t2#p#p1#sp#p1sp2.cfg $MYSQLD_DATADIR/dst/dst_t2#p#p0#sp#p0sp2.cfg

ALTER TABLE dst_t2 IMPORT PARTITION p0 TABLESPACE;
UNLOCK TABLES;

SELECT * FROM dst_t2 ORDER BY c1;

--echo # Shutdown the server to show SDI
--source include/shutdown_mysqld.inc

--echo # Dump SDI from swap table after exchange
replace_regex /version.*/VERSION/ /created.*/CREATED/
/last_altered.*/LAST_ALTERED/ /id.*/ID/ /\/\//\//;
--exec $IBD2SDI $MYSQLD_DATADIR/test/t1.ibd

--echo # Dump SDI from first partition's tablespace after exchange
replace_regex /version.*/VERSION/ /created.*/CREATED/
/last_altered.*/LAST_ALTERED/ /id.*/ID/ /\/\//\//;
--exec $IBD2SDI $MYSQLD_DATADIR/test/pt1#p#p1.ibd

--echo # Dump SDI from dst_t2's first (SDI) partition's tablespace after import.
replace_regex /version.*/VERSION/ /created.*/CREATED/
/last_altered.*/LAST_ALTERED/ /id.*/ID/ /\/\//\//;
--exec $IBD2SDI $MYSQLD_DATADIR/dst/dst_t2#p#p0#sp#p0sp0.ibd

--echo # Dump SDI from the subpartition tablespace now holding what was the first (SDI) partition of src_t2
replace_regex /version.*/VERSION/ /created.*/CREATED/
/last_altered.*/LAST_ALTERED/ /id.*/ID/ /\/\//\//;
--exec $IBD2SDI $MYSQLD_DATADIR/dst/dst_t2#p#p1#sp#p1sp0.ibd

--echo # Restart server again
--source include/start_mysqld.inc

--echo # Cleanup
USE src;
DROP TABLE src_t1;
DROP TABLE src_t2;

USE dst;
DROP TABLE dst_t1;
DROP TABLE dst_t2;

USE test;
DROP SCHEMA src;
DROP SCHEMA dst;
DROP TABLE pt1;
DROP TABLE t1;
DROP TABLE t2;

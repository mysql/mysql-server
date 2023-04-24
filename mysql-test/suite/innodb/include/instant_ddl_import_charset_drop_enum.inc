--let $MYSQLD_DATADIR = `SELECT @@datadir`

--eval CREATE TABLE t1 (  c1 INT NOT NULL,  c2 ENUM ('A', 'B', 'c', 'd') CHARACTER SET $character_set,  c3 SET ('P', 'Q', 'r', 's') CHARACTER SET $character_set,  c2drop ENUM ('A', 'B', 'c', 'd') CHARACTER SET $character_set,  c3drop SET ('P', 'Q', 'r', 's') CHARACTER SET $character_set,  c4 ENUM ('A', 'B', 'c', 'd', '') CHARACTER SET $character_set DEFAULT '',  c5 SET ('P', 'Q', 'r', 's', '') CHARACTER SET $character_set DEFAULT '',  c4drop ENUM ('A', 'B', 'c', 'd', '') CHARACTER SET $character_set DEFAULT '',  c5drop SET('P', 'Q', 'r', 's', '') CHARACTER SET $character_set DEFAULT '',  c6 ENUM ('A', 'B', 'c', 'd', '') CHARACTER SET $character_set NOT NULL DEFAULT 'd',  c7 SET ('P', 'Q', 'r', 's', '') CHARACTER SET $character_set NOT NULL DEFAULT 's',  c6drop ENUM ('A', 'B', 'c', 'd', '') CHARACTER SET $character_set NOT NULL DEFAULT'd',  c7drop SET ('P', 'Q', 'r', 's', '') CHARACTER SET $character_set NOT NULL DEFAULT 's',  PRIMARY KEY(c1)) CHARACTER SET=$character_set, ROW_FORMAT=$row_format;

INSERT INTO t1 VALUES (1, 'A', 'P', 'A', 'P', 'B', 'Q', 'B', 'Q', 'c', 'r', 'c', 'r');
INSERT INTO t1 VALUES (2, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT);
INSERT INTO t1 VALUES (3, NULL, NULL, NULL, NULL, '', '', '', '', '', '', '', '');
INSERT INTO t1 VALUES (4, 'd', 's', 'd', 's', 'd', 's', 'd', 's', 'd', 's', 'd', 's');

SELECT * FROM t1 ORDER BY c1;

ALTER TABLE t1 DROP COLUMN c2drop, DROP COLUMN c3drop, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN c4drop, DROP COLUMN c5drop, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN c6drop, DROP COLUMN c7drop, ALGORITHM=INSTANT;

--echo # Table being exported:
SELECT * FROM t1 ORDER BY c1;

CREATE TABLE t2 LIKE t1;
ALTER TABLE t2 DISCARD TABLESPACE;

--eval CREATE TABLE t3 CHARACTER SET=$character_set, ROW_FORMAT=$row_format AS SELECT * FROM t1 ORDER BY c1;
ALTER TABLE t3 ADD PRIMARY KEY (c1);
ALTER TABLE t3 DISCARD TABLESPACE;

--eval CREATE TABLE t4 (  c1 INT NOT NULL,  c2 ENUM ('A', 'B', 'c', 'd') CHARACTER SET $character_set,  c3 SET ('P', 'Q', 'r', 's') CHARACTER SET $character_set,  c2drop ENUM ('A', 'B', 'c', 'd') CHARACTER SET $character_set,  c3drop SET ('P', 'Q', 'r', 's') CHARACTER SET $character_set,  c4 ENUM ('A', 'B', 'c', 'd', '') CHARACTER SET $character_set DEFAULT '',  c5 SET ('P', 'Q', 'r', 's', '') CHARACTER SET $character_set DEFAULT '',  c4drop ENUM ('A', 'B', 'c', 'd', '') CHARACTER SET $character_set DEFAULT '',  c5drop SET('P', 'Q', 'r', 's', '') CHARACTER SET $character_set DEFAULT '',  c6 ENUM ('A', 'B', 'c', 'd', '') CHARACTER SET $character_set NOT NULL DEFAULT 'd',  c7 SET ('P', 'Q', 'r', 's', '') CHARACTER SET $character_set NOT NULL DEFAULT 's',  c6drop ENUM ('A', 'B', 'c', 'd', '') CHARACTER SET $character_set NOT NULL DEFAULT'd',  c7drop SET ('P', 'Q', 'r', 's', '') CHARACTER SET $character_set NOT NULL DEFAULT 's',  PRIMARY KEY(c1)) CHARACTER SET=$character_set, ROW_FORMAT=$row_format;

ALTER TABLE t4 DROP COLUMN c2drop, DROP COLUMN c3drop, ALGORITHM=INSTANT;
ALTER TABLE t4 DROP COLUMN c4drop, DROP COLUMN c5drop, ALGORITHM=INSTANT;
ALTER TABLE t4 DROP COLUMN c6drop, DROP COLUMN c7drop, ALGORITHM=INSTANT;
ALTER TABLE t4 DISCARD TABLESPACE;

FLUSH TABLE t1 FOR EXPORT;
--copy_file $MYSQLD_DATADIR/test/t1.cfg $MYSQLD_DATADIR/test/t2.cfg
--copy_file $MYSQLD_DATADIR/test/t1.cfg $MYSQLD_DATADIR/test/t3.cfg
--copy_file $MYSQLD_DATADIR/test/t1.cfg $MYSQLD_DATADIR/test/t4.cfg

--copy_file $MYSQLD_DATADIR/test/t1.ibd $MYSQLD_DATADIR/test/t2.ibd
--copy_file $MYSQLD_DATADIR/test/t1.ibd $MYSQLD_DATADIR/test/t3.ibd
--copy_file $MYSQLD_DATADIR/test/t1.ibd $MYSQLD_DATADIR/test/t4.ibd
UNLOCK TABLE;

ALTER TABLE t2 IMPORT TABLESPACE;
ALTER TABLE t3 IMPORT TABLESPACE;
ALTER TABLE t4 IMPORT TABLESPACE;

--echo # Table with LIKE after IMPORT:
SELECT * FROM t2 ORDER BY c1;
CHECK TABLE t2;

--echo # Table with SELECT * after IMPORT:
SELECT * FROM t3 ORDER BY c1;
CHECK TABLE t3;

--echo # Table with same modification history:
SELECT * FROM t4 ORDER BY c1;
CHECK TABLE t4;

DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;

## create tables and tablespaces without absolute paths
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' Engine=InnoDB;
CREATE TABLE tab1(c1 int, c2 varchar(10)) TABLESPACE=ts1;
INSERT INTO tab1 VALUES(1, 'VISH');
CREATE TABLE tab2(c1 int, c2 varchar(10)) Engine=InnoDB;
INSERT INTO tab2 VALUES(2, 'VISH');
CREATE INDEX ix1 ON tab1(c2) USING BTREE;
CREATE INDEX ix1 ON tab2(c2) ;
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';
CREATE UNDO TABLESPACE undo_004 ADD DATAFILE 'undo_004.ibu';
SELECT  file_name,file_type,tablespace_name FROM 
INFORMATION_SCHEMA.FILES 
WHERE FILE_TYPE LIKE '%undo%';
FILE_NAME	FILE_TYPE	TABLESPACE_NAME
./undo_001	UNDO LOG	innodb_undo_001
./undo_002	UNDO LOG	innodb_undo_002
./undo_003.ibu	UNDO LOG	undo_003
./undo_004.ibu	UNDO LOG	undo_004
## Stop DB server
# Copy whole --datadir files into new location
## Start with new --datadir
## Check datadir started with new one
SELECT @@datadir;
@@datadir
NEW_DATADIR/
## Check with SHOW
SHOW CREATE TABLE tab1;
Table	Create Table
tab1	CREATE TABLE `tab1` (
  `c1` int DEFAULT NULL,
  `c2` varchar(10) DEFAULT NULL,
  KEY `ix1` (`c2`) USING BTREE
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE tab2;
Table	Create Table
tab2	CREATE TABLE `tab2` (
  `c1` int DEFAULT NULL,
  `c2` varchar(10) DEFAULT NULL,
  KEY `ix1` (`c2`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
## Check with DML and DDL
SELECT * FROM tab1;
c1	c2
1	VISH
SELECT * FROM tab2;
c1	c2
2	VISH
ALTER UNDO TABLESPACE innodb_undo_001 SET INACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET INACTIVE;
SELECT name,space_type,state 
FROM INFORMATION_SCHEMA.INNODB_TABLESPACES 
WHERE space_type = 'Undo';
name	space_type	state
innodb_undo_001	Undo	empty
innodb_undo_002	Undo	empty
undo_003	Undo	active
undo_004	Undo	active
DELETE FROM tab1;
DELETE FROM tab2;
ALTER TABLE tab1 add column c3 varchar(10);
INSERT INTO tab1 VALUES(1, 'VISH','NATH');
INSERT INTO tab2 VALUES(2, 'VISH');
SELECT * FROM tab1;
c1	c2	c3
1	VISH	NATH
SELECT * FROM tab2;
c1	c2
2	VISH
SELECT name,space_type,state 
FROM INFORMATION_SCHEMA.INNODB_TABLESPACES 
WHERE space_type = 'Undo';
name	space_type	state
innodb_undo_001	Undo	empty
innodb_undo_002	Undo	empty
undo_003	Undo	active
undo_004	Undo	active
ALTER UNDO TABLESPACE innodb_undo_001 SET ACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET ACTIVE;
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
SELECT name,space_type,state 
FROM INFORMATION_SCHEMA.INNODB_TABLESPACES 
WHERE space_type = 'Undo';
name	space_type	state
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	empty
undo_004	Undo	empty
# Clean up new datadir
DROP TABLE tab1;
DROP TABLE tab2;
DROP TABLESPACE ts1;
DROP UNDO TABLESPACE undo_003;
DROP UNDO TABLESPACE undo_004;
# Stop DB server with new datadir
# Re-start with old --datadir
# restart: --datadir=OLD_DATADIR
# Check --datadir started with old
SELECT @@datadir;
@@datadir
OLD_DATADIR
DROP TABLE tab1;
DROP TABLE tab2;
DROP TABLESPACE ts1;
SELECT name,space_type,state 
FROM INFORMATION_SCHEMA.INNODB_TABLESPACES 
WHERE space_type = 'Undo';
name	space_type	state
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	active
undo_004	Undo	active
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
SELECT name,space_type,state 
FROM INFORMATION_SCHEMA.INNODB_TABLESPACES 
WHERE space_type = 'Undo';
name	space_type	state
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	empty
undo_004	Undo	empty
DROP UNDO TABLESPACE undo_003;
DROP UNDO TABLESPACE undo_004;

#
# Import regular and partitioned tablespaces exported from 5.7 after a restart.
#
CREATE SCHEMA `import80`;
USE `import80`;
CREATE TABLE `t1` (`c1` INT);
CREATE TABLE `t2` (`c1` INT PRIMARY KEY, `c2` INT) DATA DIRECTORY ='MYSQL_TMP_DIR/external_dir';
CREATE TABLE `t3` (`c1` INT) PARTITION BY KEY (`c1`) PARTITIONS 2;
CREATE TABLE `t4` (`c1` INT PRIMARY KEY, `c2` INT) DATA DIRECTORY ='MYSQL_TMP_DIR/external_dir' PARTITION BY HASH (`c1`) PARTITIONS 2;
CREATE TABLE `t5` (`c1` INT);
INSERT INTO `t5` values (1);
CREATE TABLE `t6` (`c1` INT PRIMARY KEY, `c2` INT) DATA DIRECTORY ='MYSQL_TMP_DIR/external_dir';
INSERT INTO `t6` values (1, 1);
CREATE TABLE `t7` (`c1` INT) PARTITION BY KEY (`c1`) PARTITIONS 2;
INSERT INTO `t7` values (1);
CREATE TABLE `t8` (`c1` INT PRIMARY KEY, `c2` INT) DATA DIRECTORY ='MYSQL_TMP_DIR/external_dir' PARTITION BY HASH (`c1`) PARTITIONS 2;
INSERT INTO `t8` values (1, 1);
FLUSH TABLES `t5`, `t6`, `t7`, `t8` FOR EXPORT;
# Copy cfg and ibd files for `t5` to MYSQL_TMP_DIR/export80.
# Copy cfg and ibd files for `t6` to MYSQL_TMP_DIR/export80.
# Copy cfg and ibd files for `t7` to MYSQL_TMP_DIR/export80.
# Copy cfg and ibd files for `t8` to MYSQL_TMP_DIR/export80.
UNLOCK TABLES;
ALTER TABLE `t1` DISCARD TABLESPACE;
ALTER TABLE `t2` DISCARD TABLESPACE;
ALTER TABLE `t3` DISCARD TABLESPACE;
ALTER TABLE `t4` DISCARD TABLESPACE;
ALTER TABLE `t5` DISCARD TABLESPACE;
ALTER TABLE `t6` DISCARD TABLESPACE;
ALTER TABLE `t7` DISCARD TABLESPACE;
ALTER TABLE `t8` DISCARD TABLESPACE;
# Unzip exported 5.7 partitioned tables to MYSQL_TMP_DIR.
# Copy exported v5.7 t1 files to MYSQLD_DATADIR/import80.
# Copy exported v5.7 t2 files to MYSQL_TMP_DIR/external_dir/import80.
# Copy exported v5.7 t3 files to MYSQLD_DATADIR/import80.
# Copy exported v5.7 t4 files to MYSQL_TMP_DIR/external_dir/import80.
# Copy exported v8.0 t5 files to MYSQLD_DATADIR/import80.
# Copy exported v8.0 t6 files to MYSQL_TMP_DIR/external_dir/import80.
# Copy exported v8.0 t7 files to MYSQLD_DATADIR/import80.
# Copy exported v8.0 t8 files to MYSQL_TMP_DIR/external_dir/import80.
# restart
ALTER TABLE `t1` IMPORT TABLESPACE;
select * from t1;
c1
1
ALTER TABLE `t2` IMPORT TABLESPACE;
select * from t2;
c1	c2
1	1
ALTER TABLE `t3` IMPORT TABLESPACE;
select * from t3;
c1
1
ALTER TABLE `t4` IMPORT TABLESPACE;
select * from t4;
c1	c2
1	1
ALTER TABLE `t5` IMPORT TABLESPACE;
select * from t5;
c1
1
ALTER TABLE `t6` IMPORT TABLESPACE;
select * from t6;
c1	c2
1	1
ALTER TABLE `t7` IMPORT TABLESPACE;
select * from t7;
c1
1
ALTER TABLE `t8` IMPORT TABLESPACE;
select * from t8;
c1	c2
1	1
# Cleanup
DROP TABLE `t1`, `t2`, t3, t4, `t5`, `t6`, t7, t8;
USE `test`;
DROP SCHEMA `import80`;

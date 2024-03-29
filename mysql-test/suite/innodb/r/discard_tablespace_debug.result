#
# Bug#33319149: TABLE ID is not updated in dd::Column se_private_date
# for discarded table
#
# Test for non-partitioned table
CREATE TABLE t1 (c1 INT, c2 INT as (c1 + 1) VIRTUAL);
SELECT TABLE_ID INTO @old_tableid FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME
LIKE '%t1%';
SET SESSION debug= '+d,skip_dd_table_access_check';
SET @query = CONCAT("SELECT COUNT(*) FROM mysql.columns WHERE SE_PRIVATE_DATA like \"%table_id=", @old_tableid, "%\"");
PREPARE stmt FROM @query;
EXECUTE stmt;
COUNT(*)
5
DEALLOCATE PREPARE stmt;
ALTER TABLE t1 DISCARD TABLESPACE;
SELECT TABLE_ID INTO @new_tableid FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME
LIKE '%t1%';
SET @query = CONCAT("SELECT COUNT(*) FROM mysql.columns WHERE SE_PRIVATE_DATA like \"%table_id=", @new_tableid, "%\"");
PREPARE stmt FROM @query;
EXECUTE stmt;
COUNT(*)
5
DEALLOCATE PREPARE stmt;
DROP TABLE t1;
# Test for partitioned table
CREATE TABLE tpart (c1 INT, c2 INT as (c1 + 1) VIRTUAL)
PARTITION BY RANGE (c1) (
PARTITION p1 VALUES LESS THAN (10),
PARTITION p2 VALUES LESS THAN (100));
SELECT TABLE_ID INTO @old_tableid FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME
LIKE '%tpart%' LIMIT 1;
SET @query = CONCAT("SELECT COUNT(*) FROM mysql.columns WHERE SE_PRIVATE_DATA like \"%table_id=", @old_tableid, "%\"");
PREPARE stmt FROM @query;
EXECUTE stmt;
COUNT(*)
5
DEALLOCATE PREPARE stmt;
ALTER TABLE tpart DISCARD TABLESPACE;
SELECT TABLE_ID INTO @new_tableid FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME
LIKE '%tpart%' LIMIT 1;
SET @query = CONCAT("SELECT COUNT(*) FROM mysql.columns WHERE SE_PRIVATE_DATA like \"%table_id=", @new_tableid, "%\"");
PREPARE stmt FROM @query;
EXECUTE stmt;
COUNT(*)
5
DEALLOCATE PREPARE stmt;
SET SESSION debug= '-d,skip_dd_table_access_check';
DROP TABLE tpart;

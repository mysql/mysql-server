CREATE TABLE tab(a BIGINT PRIMARY KEY,c1 TINYTEXT,c2 TEXT,c3 MEDIUMTEXT,
c4 TINYBLOB,c5 BLOB,c6 MEDIUMBLOB,c7 LONGBLOB) charset latin1 ENGINE=InnoDB STATS_PERSISTENT=0;
CREATE INDEX index1 ON tab(c1(255)) COMMENT 'Check index level merge MERGE_THRESHOLD=51';
Warnings:
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
CREATE INDEX index2 ON tab(c2(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=-1';
Warnings:
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
CREATE INDEX index3 ON tab(c3(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=20';
Warnings:
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
CREATE INDEX index4 ON tab(c4(255)) COMMENT 'Check index level merge MERGE_THRESHOLD=25';
Warnings:
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
CREATE INDEX index5 ON tab(c5(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=30';
Warnings:
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
CREATE INDEX index6 ON tab(c6(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=35';
Warnings:
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
CREATE INDEX index7 ON tab(c7(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=40';
Warnings:
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
SHOW CREATE TABLE tab;
Table	Create Table
tab	CREATE TABLE `tab` (
  `a` bigint NOT NULL,
  `c1` tinytext,
  `c2` text,
  `c3` mediumtext,
  `c4` tinyblob,
  `c5` blob,
  `c6` mediumblob,
  `c7` longblob,
  PRIMARY KEY (`a`),
  KEY `index1` (`c1`(255)) COMMENT 'Check index level merge MERGE_THRESHOLD=51',
  KEY `index2` (`c2`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=-1',
  KEY `index3` (`c3`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=20',
  KEY `index4` (`c4`(255)) COMMENT 'Check index level merge MERGE_THRESHOLD=25',
  KEY `index5` (`c5`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=30',
  KEY `index6` (`c6`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=35',
  KEY `index7` (`c7`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=40'
) ENGINE=InnoDB DEFAULT CHARSET=latin1 STATS_PERSISTENT=0
SELECT t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
FROM INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
WHERE t.TABLE_ID = i.TABLE_ID AND t.NAME = 'test/tab' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
test/tab	index1	50
test/tab	index2	50
test/tab	index3	20
test/tab	index4	25
test/tab	index5	30
test/tab	index6	35
test/tab	index7	40
test/tab	PRIMARY	50
ALTER TABLE tab comment='MERGE_THRESHOLD=49';
Warnings:
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
SHOW CREATE TABLE tab;
Table	Create Table
tab	CREATE TABLE `tab` (
  `a` bigint NOT NULL,
  `c1` tinytext,
  `c2` text,
  `c3` mediumtext,
  `c4` tinyblob,
  `c5` blob,
  `c6` mediumblob,
  `c7` longblob,
  PRIMARY KEY (`a`),
  KEY `index1` (`c1`(255)) COMMENT 'Check index level merge MERGE_THRESHOLD=51',
  KEY `index2` (`c2`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=-1',
  KEY `index3` (`c3`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=20',
  KEY `index4` (`c4`(255)) COMMENT 'Check index level merge MERGE_THRESHOLD=25',
  KEY `index5` (`c5`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=30',
  KEY `index6` (`c6`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=35',
  KEY `index7` (`c7`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=40'
) ENGINE=InnoDB DEFAULT CHARSET=latin1 STATS_PERSISTENT=0 COMMENT='MERGE_THRESHOLD=49'
SELECT t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
FROM INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
WHERE t.TABLE_ID = i.TABLE_ID AND t.NAME = 'test/tab' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
test/tab	index1	49
test/tab	index2	49
test/tab	index3	20
test/tab	index4	25
test/tab	index5	30
test/tab	index6	35
test/tab	index7	40
test/tab	PRIMARY	49
ALTER TABLE tab MODIFY COLUMN c7 VARCHAR(2048) ;
Warnings:
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
SHOW CREATE TABLE tab;
Table	Create Table
tab	CREATE TABLE `tab` (
  `a` bigint NOT NULL,
  `c1` tinytext,
  `c2` text,
  `c3` mediumtext,
  `c4` tinyblob,
  `c5` blob,
  `c6` mediumblob,
  `c7` varchar(2048) DEFAULT NULL,
  PRIMARY KEY (`a`),
  KEY `index1` (`c1`(255)) COMMENT 'Check index level merge MERGE_THRESHOLD=51',
  KEY `index2` (`c2`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=-1',
  KEY `index3` (`c3`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=20',
  KEY `index4` (`c4`(255)) COMMENT 'Check index level merge MERGE_THRESHOLD=25',
  KEY `index5` (`c5`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=30',
  KEY `index6` (`c6`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=35',
  KEY `index7` (`c7`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=40'
) ENGINE=InnoDB DEFAULT CHARSET=latin1 STATS_PERSISTENT=0 COMMENT='MERGE_THRESHOLD=49'
SELECT t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
FROM INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
WHERE t.TABLE_ID = i.TABLE_ID AND t.NAME = 'test/tab' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
test/tab	index1	49
test/tab	index2	49
test/tab	index3	20
test/tab	index4	25
test/tab	index5	30
test/tab	index6	35
test/tab	index7	40
test/tab	PRIMARY	49
ALTER TABLE tab ADD INDEX index8 (c7(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=45';
Warnings:
Warning	1831	Duplicate index 'index8' defined on the table 'test.tab'. This is deprecated and will be disallowed in a future release.
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
Warning	1478	InnoDB: Invalid value for MERGE_THRESHOLD in the CREATE TABLE statement. The value is ignored.
SHOW CREATE TABLE tab;
Table	Create Table
tab	CREATE TABLE `tab` (
  `a` bigint NOT NULL,
  `c1` tinytext,
  `c2` text,
  `c3` mediumtext,
  `c4` tinyblob,
  `c5` blob,
  `c6` mediumblob,
  `c7` varchar(2048) DEFAULT NULL,
  PRIMARY KEY (`a`),
  KEY `index1` (`c1`(255)) COMMENT 'Check index level merge MERGE_THRESHOLD=51',
  KEY `index2` (`c2`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=-1',
  KEY `index3` (`c3`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=20',
  KEY `index4` (`c4`(255)) COMMENT 'Check index level merge MERGE_THRESHOLD=25',
  KEY `index5` (`c5`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=30',
  KEY `index6` (`c6`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=35',
  KEY `index7` (`c7`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=40',
  KEY `index8` (`c7`(750)) COMMENT 'Check index level merge MERGE_THRESHOLD=45'
) ENGINE=InnoDB DEFAULT CHARSET=latin1 STATS_PERSISTENT=0 COMMENT='MERGE_THRESHOLD=49'
SELECT t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
FROM INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
WHERE t.TABLE_ID = i.TABLE_ID AND t.NAME = 'test/tab' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
test/tab	index1	49
test/tab	index2	49
test/tab	index3	20
test/tab	index4	25
test/tab	index5	30
test/tab	index6	35
test/tab	index7	40
test/tab	index8	45
test/tab	PRIMARY	49
DROP TABLE tab;
#
# behavior for deleting records
#
# test to confirm behavior (MERGE_THRESHOLD=50 (default))
CREATE TABLE tab1 (a bigint primary key, b varchar(2048)) engine=InnoDB STATS_PERSISTENT=0;
# check MERGE_THRESHOLD
select t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
from INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
where t.TABLE_ID = i.TABLE_ID and t.NAME like 'test/tab1%' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
test/tab1	PRIMARY	50
insert into tab1 values (1, repeat('a',2048));
insert into tab1 values (2, repeat('a',2048));
insert into tab1 values (3, repeat('a',2048));
insert into tab1 values (8, repeat('a',2048));
insert into tab1 values (9, repeat('a',2048));
insert into tab1 values (10, repeat('a',2048));
insert into tab1 values (11, repeat('a',2048));
insert into tab1 values (12, repeat('a',2048));
insert into tab1 values (4, repeat('a',2048));
insert into tab1 values (5, repeat('a',2048));
insert into tab1 values (6, repeat('a',2048));
insert into tab1 values (7, repeat('a',2048));
insert into tab1 values (13, repeat('a',2048));
insert into tab1 values (14, repeat('a',2048));
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	2
5	7
6	7
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 12;
delete from tab1 where a = 13;
delete from tab1 where a = 14;
delete from tab1 where a = 5;
delete from tab1 where a = 6;
delete from tab1 where a = 7;
set global innodb_purge_run_now=ON;
# check page merge happens (nothing is expected)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	2
5	4
6	4
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 11;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=50 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 10;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=35 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	2
index_page_merge_successful	2
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 9;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=25 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	2
index_page_merge_successful	2
DROP TABLE tab1;
# test to confirm behavior (MERGE_THRESHOLD=35)
CREATE TABLE tab1 (a bigint primary key, b varchar(2048)) engine=InnoDB STATS_PERSISTENT=0
COMMENT='MERGE_THRESHOLD=35';
# check MERGE_THRESHOLD
select t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
from INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
where t.TABLE_ID = i.TABLE_ID and t.NAME like 'test/tab1%' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
test/tab1	PRIMARY	35
insert into tab1 values (1, repeat('a',2048));
insert into tab1 values (2, repeat('a',2048));
insert into tab1 values (3, repeat('a',2048));
insert into tab1 values (8, repeat('a',2048));
insert into tab1 values (9, repeat('a',2048));
insert into tab1 values (10, repeat('a',2048));
insert into tab1 values (11, repeat('a',2048));
insert into tab1 values (12, repeat('a',2048));
insert into tab1 values (4, repeat('a',2048));
insert into tab1 values (5, repeat('a',2048));
insert into tab1 values (6, repeat('a',2048));
insert into tab1 values (7, repeat('a',2048));
insert into tab1 values (13, repeat('a',2048));
insert into tab1 values (14, repeat('a',2048));
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	2
5	7
6	7
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 12;
delete from tab1 where a = 13;
delete from tab1 where a = 14;
delete from tab1 where a = 5;
delete from tab1 where a = 6;
delete from tab1 where a = 7;
set global innodb_purge_run_now=ON;
# check page merge happens (nothing is expected)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	2
5	4
6	4
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 11;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=50 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 10;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=35 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 9;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=25 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	2
index_page_merge_successful	2
DROP TABLE tab1;
# test to confirm behavior (MERGE_THRESHOLD=25)
CREATE TABLE tab1 (a bigint primary key, b varchar(2048)) engine=InnoDB STATS_PERSISTENT=0
COMMENT='MERGE_THRESHOLD=25';
# check MERGE_THRESHOLD
select t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
from INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
where t.TABLE_ID = i.TABLE_ID and t.NAME like 'test/tab1%' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
test/tab1	PRIMARY	25
insert into tab1 values (1, repeat('a',2048));
insert into tab1 values (2, repeat('a',2048));
insert into tab1 values (3, repeat('a',2048));
insert into tab1 values (8, repeat('a',2048));
insert into tab1 values (9, repeat('a',2048));
insert into tab1 values (10, repeat('a',2048));
insert into tab1 values (11, repeat('a',2048));
insert into tab1 values (12, repeat('a',2048));
insert into tab1 values (4, repeat('a',2048));
insert into tab1 values (5, repeat('a',2048));
insert into tab1 values (6, repeat('a',2048));
insert into tab1 values (7, repeat('a',2048));
insert into tab1 values (13, repeat('a',2048));
insert into tab1 values (14, repeat('a',2048));
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	2
5	7
6	7
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 12;
delete from tab1 where a = 13;
delete from tab1 where a = 14;
delete from tab1 where a = 5;
delete from tab1 where a = 6;
delete from tab1 where a = 7;
set global innodb_purge_run_now=ON;
# check page merge happens (nothing is expected)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	2
5	4
6	4
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 11;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=50 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 10;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=35 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 9;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=25 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
DROP TABLE tab1;
# test to confirm partitioned table (MERGE_THRESHOLD=35)
CREATE TABLE tab1 (a bigint primary key, b varchar(2048)) STATS_PERSISTENT=0
COMMENT='MERGE_THRESHOLD=35'
PARTITION BY RANGE (a)
(PARTITION p0 VALUES LESS THAN (20) ENGINE = InnoDB,
PARTITION p1 VALUES LESS THAN MAXVALUE ENGINE = InnoDB);
# check MERGE_THRESHOLD
select t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
from INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
where t.TABLE_ID = i.TABLE_ID and t.NAME like 'test/tab1%' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
insert into tab1 values (1, repeat('a',2048));
insert into tab1 values (2, repeat('a',2048));
insert into tab1 values (3, repeat('a',2048));
insert into tab1 values (8, repeat('a',2048));
insert into tab1 values (9, repeat('a',2048));
insert into tab1 values (10, repeat('a',2048));
insert into tab1 values (11, repeat('a',2048));
insert into tab1 values (12, repeat('a',2048));
insert into tab1 values (4, repeat('a',2048));
insert into tab1 values (5, repeat('a',2048));
insert into tab1 values (6, repeat('a',2048));
insert into tab1 values (7, repeat('a',2048));
insert into tab1 values (13, repeat('a',2048));
insert into tab1 values (14, repeat('a',2048));
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	0
4	2
5	7
6	7
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 12;
delete from tab1 where a = 13;
delete from tab1 where a = 14;
delete from tab1 where a = 5;
delete from tab1 where a = 6;
delete from tab1 where a = 7;
set global innodb_purge_run_now=ON;
# check page merge happens (nothing is expected)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	0
4	2
5	4
6	4
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 11;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=50 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 10;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=35 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 9;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=25 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	2
index_page_merge_successful	2
DROP TABLE tab1;
#
# behavior for updating to smaller records
#
# test to confirm behavior (MERGE_THRESHOLD=50 (default))
CREATE TABLE tab1 (a bigint primary key, b varchar(2048)) engine=InnoDB STATS_PERSISTENT=0;
# check MERGE_THRESHOLD
select t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
from INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
where t.TABLE_ID = i.TABLE_ID and t.NAME like 'test/tab1%' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
test/tab1	PRIMARY	50
insert into tab1 values (1, repeat('a',2048));
insert into tab1 values (2, repeat('a',2048));
insert into tab1 values (3, repeat('a',2048));
insert into tab1 values (8, repeat('a',2048));
insert into tab1 values (9, repeat('a',2048));
insert into tab1 values (10, repeat('a',2048));
insert into tab1 values (11, repeat('a',2048));
insert into tab1 values (12, repeat('a',2048));
insert into tab1 values (4, repeat('a',2048));
insert into tab1 values (5, repeat('a',2048));
insert into tab1 values (6, repeat('a',2048));
insert into tab1 values (7, repeat('a',2048));
insert into tab1 values (13, repeat('a',2048));
insert into tab1 values (14, repeat('a',2048));
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	2
5	7
6	7
update tab1 set b='' where a = 12;
update tab1 set b='' where a = 13;
update tab1 set b='' where a = 14;
update tab1 set b='' where a = 5;
update tab1 set b='' where a = 6;
update tab1 set b='' where a = 7;
# check page merge happens (nothing is expected)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	2
5	7
6	7
update tab1 set b='' where a = 11;
# check page merge happens (MERGE_THRESHOLD=50 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
update tab1 set b='' where a = 10;
# check page merge happens (MERGE_THRESHOLD=35 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
update tab1 set b='' where a = 9;
# check page merge happens (MERGE_THRESHOLD=25 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
DROP TABLE tab1;
# test to confirm behavior (MERGE_THRESHOLD=35)
CREATE TABLE tab1 (a bigint primary key, b varchar(2048)) engine=InnoDB
COMMENT='MERGE_THRESHOLD=35' STATS_PERSISTENT=0;
# check MERGE_THRESHOLD
select t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
from INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
where t.TABLE_ID = i.TABLE_ID and t.NAME like 'test/tab1%' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
test/tab1	PRIMARY	35
insert into tab1 values (1, repeat('a',2048));
insert into tab1 values (2, repeat('a',2048));
insert into tab1 values (3, repeat('a',2048));
insert into tab1 values (8, repeat('a',2048));
insert into tab1 values (9, repeat('a',2048));
insert into tab1 values (10, repeat('a',2048));
insert into tab1 values (11, repeat('a',2048));
insert into tab1 values (12, repeat('a',2048));
insert into tab1 values (4, repeat('a',2048));
insert into tab1 values (5, repeat('a',2048));
insert into tab1 values (6, repeat('a',2048));
insert into tab1 values (7, repeat('a',2048));
insert into tab1 values (13, repeat('a',2048));
insert into tab1 values (14, repeat('a',2048));
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	2
5	7
6	7
update tab1 set b='' where a = 12;
update tab1 set b='' where a = 13;
update tab1 set b='' where a = 14;
update tab1 set b='' where a = 5;
update tab1 set b='' where a = 6;
update tab1 set b='' where a = 7;
# check page merge happens (nothing is expected)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	2
5	7
6	7
update tab1 set b='' where a = 11;
# check page merge happens (MERGE_THRESHOLD=50 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
update tab1 set b='' where a = 10;
# check page merge happens (MERGE_THRESHOLD=35 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
update tab1 set b='' where a = 9;
# check page merge happens (MERGE_THRESHOLD=25 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
DROP TABLE tab1;
# test to confirm behavior (MERGE_THRESHOLD=25)
CREATE TABLE tab1 (a bigint primary key, b varchar(2048)) engine=InnoDB
COMMENT='MERGE_THRESHOLD=25' STATS_PERSISTENT=0;
# check MERGE_THRESHOLD
select t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
from INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
where t.TABLE_ID = i.TABLE_ID and t.NAME like 'test/tab1%' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
test/tab1	PRIMARY	25
insert into tab1 values (1, repeat('a',2048));
insert into tab1 values (2, repeat('a',2048));
insert into tab1 values (3, repeat('a',2048));
insert into tab1 values (8, repeat('a',2048));
insert into tab1 values (9, repeat('a',2048));
insert into tab1 values (10, repeat('a',2048));
insert into tab1 values (11, repeat('a',2048));
insert into tab1 values (12, repeat('a',2048));
insert into tab1 values (4, repeat('a',2048));
insert into tab1 values (5, repeat('a',2048));
insert into tab1 values (6, repeat('a',2048));
insert into tab1 values (7, repeat('a',2048));
insert into tab1 values (13, repeat('a',2048));
insert into tab1 values (14, repeat('a',2048));
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	2
5	7
6	7
update tab1 set b='' where a = 12;
update tab1 set b='' where a = 13;
update tab1 set b='' where a = 14;
update tab1 set b='' where a = 5;
update tab1 set b='' where a = 6;
update tab1 set b='' where a = 7;
# check page merge happens (nothing is expected)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	2
5	7
6	7
update tab1 set b='' where a = 11;
# check page merge happens (MERGE_THRESHOLD=50 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
update tab1 set b='' where a = 10;
# check page merge happens (MERGE_THRESHOLD=35 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
update tab1 set b='' where a = 9;
# check page merge happens (MERGE_THRESHOLD=25 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
DROP TABLE tab1;
# test to confirm explicit temporary table (MERGE_THRESHOLD=35)
# (though not registered to SYS_TABLES,SYS_INDEXES, it works correctly)
CREATE TEMPORARY TABLE tab1 (a bigint primary key, b varchar(2048)) engine=InnoDB
COMMENT='MERGE_THRESHOLD=35' STATS_PERSISTENT=0;
# check MERGE_THRESHOLD
select t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
from INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
where t.TABLE_ID = i.TABLE_ID and t.NAME like 'test/tab1%' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
insert into tab1 values (1, repeat('a',2048));
insert into tab1 values (2, repeat('a',2048));
insert into tab1 values (3, repeat('a',2048));
insert into tab1 values (8, repeat('a',2048));
insert into tab1 values (9, repeat('a',2048));
insert into tab1 values (10, repeat('a',2048));
insert into tab1 values (11, repeat('a',2048));
insert into tab1 values (12, repeat('a',2048));
insert into tab1 values (4, repeat('a',2048));
insert into tab1 values (5, repeat('a',2048));
insert into tab1 values (6, repeat('a',2048));
insert into tab1 values (7, repeat('a',2048));
insert into tab1 values (13, repeat('a',2048));
insert into tab1 values (14, repeat('a',2048));
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
update tab1 set b='' where a = 12;
update tab1 set b='' where a = 13;
update tab1 set b='' where a = 14;
update tab1 set b='' where a = 5;
update tab1 set b='' where a = 6;
update tab1 set b='' where a = 7;
# check page merge happens (nothing is expected)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
update tab1 set b='' where a = 11;
# check page merge happens (MERGE_THRESHOLD=50 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
update tab1 set b='' where a = 10;
# check page merge happens (MERGE_THRESHOLD=35 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
update tab1 set b='' where a = 9;
# check page merge happens (MERGE_THRESHOLD=25 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
DROP TABLE tab1;
#
# behavior for secondary index with blob
#
# test to confirm behavior (MERGE_THRESHOLD=50 (default))
CREATE TABLE tab1 (a bigint primary key, b blob) engine=InnoDB row_format=dynamic STATS_PERSISTENT=0;
CREATE INDEX index1 ON tab1(b(750));
# check MERGE_THRESHOLD
select t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
from INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
where t.TABLE_ID = i.TABLE_ID and t.NAME like 'test/tab1%' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
test/tab1	index1	50
test/tab1	PRIMARY	50
INSERT INTO tab1 VALUES (1, concat("01", repeat('a',8190)));
INSERT INTO tab1 VALUES (2, concat("02", repeat('a',8190)));
INSERT INTO tab1 VALUES (3, concat("03", repeat('a',8190)));
INSERT INTO tab1 VALUES (4, concat("04", repeat('a',8190)));
INSERT INTO tab1 VALUES (5, concat("05", repeat('a',8190)));
INSERT INTO tab1 VALUES (6, concat("06", repeat('a',8190)));
INSERT INTO tab1 VALUES (7, concat("07", repeat('a',8190)));
INSERT INTO tab1 VALUES (8, concat("08", repeat('a',8190)));
INSERT INTO tab1 VALUES (9, concat("09", repeat('a',8190)));
INSERT INTO tab1 VALUES (10, concat("10", repeat('a',8190)));
INSERT INTO tab1 VALUES (22, concat("22", repeat('a',8190)));
INSERT INTO tab1 VALUES (23, concat("23", repeat('a',8190)));
INSERT INTO tab1 VALUES (24, concat("24", repeat('a',8190)));
INSERT INTO tab1 VALUES (25, concat("25", repeat('a',8190)));
INSERT INTO tab1 VALUES (26, concat("26", repeat('a',8190)));
INSERT INTO tab1 VALUES (27, concat("27", repeat('a',8190)));
INSERT INTO tab1 VALUES (28, concat("28", repeat('a',8190)));
INSERT INTO tab1 VALUES (29, concat("29", repeat('a',8190)));
INSERT INTO tab1 VALUES (30, concat("30", repeat('a',8190)));
INSERT INTO tab1 VALUES (31, concat("31", repeat('a',8190)));
INSERT INTO tab1 VALUES (32, concat("32", repeat('a',8190)));
INSERT INTO tab1 VALUES (33, concat("33", repeat('a',8190)));
INSERT INTO tab1 VALUES (11, concat("11", repeat('a',8190)));
INSERT INTO tab1 VALUES (12, concat("12", repeat('a',8190)));
INSERT INTO tab1 VALUES (13, concat("13", repeat('a',8190)));
INSERT INTO tab1 VALUES (14, concat("14", repeat('a',8190)));
INSERT INTO tab1 VALUES (15, concat("15", repeat('a',8190)));
INSERT INTO tab1 VALUES (16, concat("16", repeat('a',8190)));
INSERT INTO tab1 VALUES (17, concat("17", repeat('a',8190)));
INSERT INTO tab1 VALUES (18, concat("18", repeat('a',8190)));
INSERT INTO tab1 VALUES (19, concat("19", repeat('a',8190)));
INSERT INTO tab1 VALUES (20, concat("20", repeat('a',8190)));
INSERT INTO tab1 VALUES (21, concat("21", repeat('a',8190)));
INSERT INTO tab1 VALUES (34, concat("34", repeat('a',8190)));
INSERT INTO tab1 VALUES (35, concat("35", repeat('a',8190)));
INSERT INTO tab1 VALUES (36, concat("36", repeat('a',8190)));
INSERT INTO tab1 VALUES (37, concat("37", repeat('a',8190)));
INSERT INTO tab1 VALUES (38, concat("38", repeat('a',8190)));
INSERT INTO tab1 VALUES (39, concat("39", repeat('a',8190)));
INSERT INTO tab1 VALUES (40, concat("40", repeat('a',8190)));
INSERT INTO tab1 VALUES (41, concat("41", repeat('a',8190)));
INSERT INTO tab1 VALUES (42, concat("42", repeat('a',8190)));
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	42
5	2
28	21
29	21
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 33;
delete from tab1 where a = 34;
delete from tab1 where a = 35;
delete from tab1 where a = 36;
delete from tab1 where a = 37;
delete from tab1 where a = 38;
delete from tab1 where a = 39;
delete from tab1 where a = 40;
delete from tab1 where a = 41;
delete from tab1 where a = 42;
delete from tab1 where a = 12;
delete from tab1 where a = 13;
delete from tab1 where a = 14;
delete from tab1 where a = 15;
delete from tab1 where a = 16;
delete from tab1 where a = 17;
delete from tab1 where a = 18;
delete from tab1 where a = 19;
delete from tab1 where a = 20;
delete from tab1 where a = 21;
set global innodb_purge_run_now=ON;
# check page merge happens (nothing is expected)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	22
5	2
28	11
29	11
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 32;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=50 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 31;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=45 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	2
index_page_merge_successful	2
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 30;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=40 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	2
index_page_merge_successful	2
DROP TABLE tab1;
# test to confirm behavior (MERGE_THRESHOLD=45)
CREATE TABLE tab1 (a bigint primary key, b blob) engine=InnoDB row_format=dynamic STATS_PERSISTENT=0;
CREATE INDEX index1 ON tab1(b(750)) COMMENT 'MERGE_THRESHOLD=45';
# check MERGE_THRESHOLD
select t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
from INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
where t.TABLE_ID = i.TABLE_ID and t.NAME like 'test/tab1%' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
test/tab1	index1	45
test/tab1	PRIMARY	50
INSERT INTO tab1 VALUES (1, concat("01", repeat('a',8190)));
INSERT INTO tab1 VALUES (2, concat("02", repeat('a',8190)));
INSERT INTO tab1 VALUES (3, concat("03", repeat('a',8190)));
INSERT INTO tab1 VALUES (4, concat("04", repeat('a',8190)));
INSERT INTO tab1 VALUES (5, concat("05", repeat('a',8190)));
INSERT INTO tab1 VALUES (6, concat("06", repeat('a',8190)));
INSERT INTO tab1 VALUES (7, concat("07", repeat('a',8190)));
INSERT INTO tab1 VALUES (8, concat("08", repeat('a',8190)));
INSERT INTO tab1 VALUES (9, concat("09", repeat('a',8190)));
INSERT INTO tab1 VALUES (10, concat("10", repeat('a',8190)));
INSERT INTO tab1 VALUES (22, concat("22", repeat('a',8190)));
INSERT INTO tab1 VALUES (23, concat("23", repeat('a',8190)));
INSERT INTO tab1 VALUES (24, concat("24", repeat('a',8190)));
INSERT INTO tab1 VALUES (25, concat("25", repeat('a',8190)));
INSERT INTO tab1 VALUES (26, concat("26", repeat('a',8190)));
INSERT INTO tab1 VALUES (27, concat("27", repeat('a',8190)));
INSERT INTO tab1 VALUES (28, concat("28", repeat('a',8190)));
INSERT INTO tab1 VALUES (29, concat("29", repeat('a',8190)));
INSERT INTO tab1 VALUES (30, concat("30", repeat('a',8190)));
INSERT INTO tab1 VALUES (31, concat("31", repeat('a',8190)));
INSERT INTO tab1 VALUES (32, concat("32", repeat('a',8190)));
INSERT INTO tab1 VALUES (33, concat("33", repeat('a',8190)));
INSERT INTO tab1 VALUES (11, concat("11", repeat('a',8190)));
INSERT INTO tab1 VALUES (12, concat("12", repeat('a',8190)));
INSERT INTO tab1 VALUES (13, concat("13", repeat('a',8190)));
INSERT INTO tab1 VALUES (14, concat("14", repeat('a',8190)));
INSERT INTO tab1 VALUES (15, concat("15", repeat('a',8190)));
INSERT INTO tab1 VALUES (16, concat("16", repeat('a',8190)));
INSERT INTO tab1 VALUES (17, concat("17", repeat('a',8190)));
INSERT INTO tab1 VALUES (18, concat("18", repeat('a',8190)));
INSERT INTO tab1 VALUES (19, concat("19", repeat('a',8190)));
INSERT INTO tab1 VALUES (20, concat("20", repeat('a',8190)));
INSERT INTO tab1 VALUES (21, concat("21", repeat('a',8190)));
INSERT INTO tab1 VALUES (34, concat("34", repeat('a',8190)));
INSERT INTO tab1 VALUES (35, concat("35", repeat('a',8190)));
INSERT INTO tab1 VALUES (36, concat("36", repeat('a',8190)));
INSERT INTO tab1 VALUES (37, concat("37", repeat('a',8190)));
INSERT INTO tab1 VALUES (38, concat("38", repeat('a',8190)));
INSERT INTO tab1 VALUES (39, concat("39", repeat('a',8190)));
INSERT INTO tab1 VALUES (40, concat("40", repeat('a',8190)));
INSERT INTO tab1 VALUES (41, concat("41", repeat('a',8190)));
INSERT INTO tab1 VALUES (42, concat("42", repeat('a',8190)));
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	42
5	2
28	21
29	21
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 33;
delete from tab1 where a = 34;
delete from tab1 where a = 35;
delete from tab1 where a = 36;
delete from tab1 where a = 37;
delete from tab1 where a = 38;
delete from tab1 where a = 39;
delete from tab1 where a = 40;
delete from tab1 where a = 41;
delete from tab1 where a = 42;
delete from tab1 where a = 12;
delete from tab1 where a = 13;
delete from tab1 where a = 14;
delete from tab1 where a = 15;
delete from tab1 where a = 16;
delete from tab1 where a = 17;
delete from tab1 where a = 18;
delete from tab1 where a = 19;
delete from tab1 where a = 20;
delete from tab1 where a = 21;
set global innodb_purge_run_now=ON;
# check page merge happens (nothing is expected)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	22
5	2
28	11
29	11
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 32;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=50 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 31;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=45 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 30;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=40 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	2
index_page_merge_successful	2
DROP TABLE tab1;
# test to confirm behavior (MERGE_THRESHOLD=40)
CREATE TABLE tab1 (a bigint primary key, b blob) engine=InnoDB row_format=dynamic STATS_PERSISTENT=0;
CREATE INDEX index1 ON tab1(b(750)) COMMENT 'MERGE_THRESHOLD=40';
# check MERGE_THRESHOLD
select t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
from INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
where t.TABLE_ID = i.TABLE_ID and t.NAME like 'test/tab1%' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
test/tab1	index1	40
test/tab1	PRIMARY	50
INSERT INTO tab1 VALUES (1, concat("01", repeat('a',8190)));
INSERT INTO tab1 VALUES (2, concat("02", repeat('a',8190)));
INSERT INTO tab1 VALUES (3, concat("03", repeat('a',8190)));
INSERT INTO tab1 VALUES (4, concat("04", repeat('a',8190)));
INSERT INTO tab1 VALUES (5, concat("05", repeat('a',8190)));
INSERT INTO tab1 VALUES (6, concat("06", repeat('a',8190)));
INSERT INTO tab1 VALUES (7, concat("07", repeat('a',8190)));
INSERT INTO tab1 VALUES (8, concat("08", repeat('a',8190)));
INSERT INTO tab1 VALUES (9, concat("09", repeat('a',8190)));
INSERT INTO tab1 VALUES (10, concat("10", repeat('a',8190)));
INSERT INTO tab1 VALUES (22, concat("22", repeat('a',8190)));
INSERT INTO tab1 VALUES (23, concat("23", repeat('a',8190)));
INSERT INTO tab1 VALUES (24, concat("24", repeat('a',8190)));
INSERT INTO tab1 VALUES (25, concat("25", repeat('a',8190)));
INSERT INTO tab1 VALUES (26, concat("26", repeat('a',8190)));
INSERT INTO tab1 VALUES (27, concat("27", repeat('a',8190)));
INSERT INTO tab1 VALUES (28, concat("28", repeat('a',8190)));
INSERT INTO tab1 VALUES (29, concat("29", repeat('a',8190)));
INSERT INTO tab1 VALUES (30, concat("30", repeat('a',8190)));
INSERT INTO tab1 VALUES (31, concat("31", repeat('a',8190)));
INSERT INTO tab1 VALUES (32, concat("32", repeat('a',8190)));
INSERT INTO tab1 VALUES (33, concat("33", repeat('a',8190)));
INSERT INTO tab1 VALUES (11, concat("11", repeat('a',8190)));
INSERT INTO tab1 VALUES (12, concat("12", repeat('a',8190)));
INSERT INTO tab1 VALUES (13, concat("13", repeat('a',8190)));
INSERT INTO tab1 VALUES (14, concat("14", repeat('a',8190)));
INSERT INTO tab1 VALUES (15, concat("15", repeat('a',8190)));
INSERT INTO tab1 VALUES (16, concat("16", repeat('a',8190)));
INSERT INTO tab1 VALUES (17, concat("17", repeat('a',8190)));
INSERT INTO tab1 VALUES (18, concat("18", repeat('a',8190)));
INSERT INTO tab1 VALUES (19, concat("19", repeat('a',8190)));
INSERT INTO tab1 VALUES (20, concat("20", repeat('a',8190)));
INSERT INTO tab1 VALUES (21, concat("21", repeat('a',8190)));
INSERT INTO tab1 VALUES (34, concat("34", repeat('a',8190)));
INSERT INTO tab1 VALUES (35, concat("35", repeat('a',8190)));
INSERT INTO tab1 VALUES (36, concat("36", repeat('a',8190)));
INSERT INTO tab1 VALUES (37, concat("37", repeat('a',8190)));
INSERT INTO tab1 VALUES (38, concat("38", repeat('a',8190)));
INSERT INTO tab1 VALUES (39, concat("39", repeat('a',8190)));
INSERT INTO tab1 VALUES (40, concat("40", repeat('a',8190)));
INSERT INTO tab1 VALUES (41, concat("41", repeat('a',8190)));
INSERT INTO tab1 VALUES (42, concat("42", repeat('a',8190)));
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	42
5	2
28	21
29	21
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 33;
delete from tab1 where a = 34;
delete from tab1 where a = 35;
delete from tab1 where a = 36;
delete from tab1 where a = 37;
delete from tab1 where a = 38;
delete from tab1 where a = 39;
delete from tab1 where a = 40;
delete from tab1 where a = 41;
delete from tab1 where a = 42;
delete from tab1 where a = 12;
delete from tab1 where a = 13;
delete from tab1 where a = 14;
delete from tab1 where a = 15;
delete from tab1 where a = 16;
delete from tab1 where a = 17;
delete from tab1 where a = 18;
delete from tab1 where a = 19;
delete from tab1 where a = 20;
delete from tab1 where a = 21;
set global innodb_purge_run_now=ON;
# check page merge happens (nothing is expected)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	22
5	2
28	11
29	11
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 32;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=50 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 31;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=45 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 30;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=40 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
DROP TABLE tab1;
# compressed table behaves same (MERGE_THRESHOLD=45)
CREATE TABLE tab1 (a bigint primary key, b blob) engine=InnoDB STATS_PERSISTENT=0
ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=8;
CREATE INDEX index1 ON tab1(b(750)) COMMENT 'MERGE_THRESHOLD=45';
# check MERGE_THRESHOLD
select t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
from INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
where t.TABLE_ID = i.TABLE_ID and t.NAME like 'test/tab1%' ORDER BY i.NAME;
TABLE_NAME	INDEX_NAME	MERGE_THRESHOLD
test/tab1	index1	45
test/tab1	PRIMARY	50
INSERT INTO tab1 VALUES (1, concat("01", repeat('a',8190)));
INSERT INTO tab1 VALUES (2, concat("02", repeat('a',8190)));
INSERT INTO tab1 VALUES (3, concat("03", repeat('a',8190)));
INSERT INTO tab1 VALUES (4, concat("04", repeat('a',8190)));
INSERT INTO tab1 VALUES (5, concat("05", repeat('a',8190)));
INSERT INTO tab1 VALUES (6, concat("06", repeat('a',8190)));
INSERT INTO tab1 VALUES (7, concat("07", repeat('a',8190)));
INSERT INTO tab1 VALUES (8, concat("08", repeat('a',8190)));
INSERT INTO tab1 VALUES (9, concat("09", repeat('a',8190)));
INSERT INTO tab1 VALUES (10, concat("10", repeat('a',8190)));
INSERT INTO tab1 VALUES (22, concat("22", repeat('a',8190)));
INSERT INTO tab1 VALUES (23, concat("23", repeat('a',8190)));
INSERT INTO tab1 VALUES (24, concat("24", repeat('a',8190)));
INSERT INTO tab1 VALUES (25, concat("25", repeat('a',8190)));
INSERT INTO tab1 VALUES (26, concat("26", repeat('a',8190)));
INSERT INTO tab1 VALUES (27, concat("27", repeat('a',8190)));
INSERT INTO tab1 VALUES (28, concat("28", repeat('a',8190)));
INSERT INTO tab1 VALUES (29, concat("29", repeat('a',8190)));
INSERT INTO tab1 VALUES (30, concat("30", repeat('a',8190)));
INSERT INTO tab1 VALUES (31, concat("31", repeat('a',8190)));
INSERT INTO tab1 VALUES (32, concat("32", repeat('a',8190)));
INSERT INTO tab1 VALUES (33, concat("33", repeat('a',8190)));
INSERT INTO tab1 VALUES (11, concat("11", repeat('a',8190)));
INSERT INTO tab1 VALUES (12, concat("12", repeat('a',8190)));
INSERT INTO tab1 VALUES (13, concat("13", repeat('a',8190)));
INSERT INTO tab1 VALUES (14, concat("14", repeat('a',8190)));
INSERT INTO tab1 VALUES (15, concat("15", repeat('a',8190)));
INSERT INTO tab1 VALUES (16, concat("16", repeat('a',8190)));
INSERT INTO tab1 VALUES (17, concat("17", repeat('a',8190)));
INSERT INTO tab1 VALUES (18, concat("18", repeat('a',8190)));
INSERT INTO tab1 VALUES (19, concat("19", repeat('a',8190)));
INSERT INTO tab1 VALUES (20, concat("20", repeat('a',8190)));
INSERT INTO tab1 VALUES (21, concat("21", repeat('a',8190)));
INSERT INTO tab1 VALUES (34, concat("34", repeat('a',8190)));
INSERT INTO tab1 VALUES (35, concat("35", repeat('a',8190)));
INSERT INTO tab1 VALUES (36, concat("36", repeat('a',8190)));
INSERT INTO tab1 VALUES (37, concat("37", repeat('a',8190)));
INSERT INTO tab1 VALUES (38, concat("38", repeat('a',8190)));
INSERT INTO tab1 VALUES (39, concat("39", repeat('a',8190)));
INSERT INTO tab1 VALUES (40, concat("40", repeat('a',8190)));
INSERT INTO tab1 VALUES (41, concat("41", repeat('a',8190)));
INSERT INTO tab1 VALUES (42, concat("42", repeat('a',8190)));
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	42
5	2
28	21
29	21
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 33;
delete from tab1 where a = 34;
delete from tab1 where a = 35;
delete from tab1 where a = 36;
delete from tab1 where a = 37;
delete from tab1 where a = 38;
delete from tab1 where a = 39;
delete from tab1 where a = 40;
delete from tab1 where a = 41;
delete from tab1 where a = 42;
delete from tab1 where a = 12;
delete from tab1 where a = 13;
delete from tab1 where a = 14;
delete from tab1 where a = 15;
delete from tab1 where a = 16;
delete from tab1 where a = 17;
delete from tab1 where a = 18;
delete from tab1 where a = 19;
delete from tab1 where a = 20;
delete from tab1 where a = 21;
set global innodb_purge_run_now=ON;
# check page merge happens (nothing is expected)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;
PAGE_NUMBER	NUMBER_RECORDS
4	22
5	2
28	11
29	11
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 32;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=50 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	0
index_page_merge_successful	0
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 31;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=45 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	1
index_page_merge_successful	1
set global innodb_purge_stop_now=ON;
delete from tab1 where a = 30;
set global innodb_purge_run_now=ON;
# check page merge happens (MERGE_THRESHOLD=40 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';
name	count_reset
index_page_merge_attempts	2
index_page_merge_successful	2
DROP TABLE tab1;

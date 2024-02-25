#
# Test to cause merge of the pages (at secondary index by deleting)
# test/tab1 should be created already with innodb_file_per_table=ON
# The definition is intended to be based on
# "create table tab1 (a bigint primary key, b blob) engine=InnoDB row_format=dynamic;"
# "create index index1 on tab1(b(750));"
#

--echo # check MERGE_THRESHOLD
select t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
from INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
where t.TABLE_ID = i.TABLE_ID and t.NAME like 'test/tab1%' ORDER BY i.NAME;

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

# clustered index is still root page only with the 42 records.
# secondary index is filled 2 leaf pages have been prepared
# | 1,..,21 | 22,..,42 |

select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;

# turn on flags
--disable_query_log
SET GLOBAL innodb_monitor_enable=index_page_merge_attempts;
SET GLOBAL innodb_monitor_reset=index_page_merge_attempts;
SET GLOBAL innodb_monitor_enable=index_page_merge_successful;
SET GLOBAL innodb_monitor_reset=index_page_merge_successful;
--enable_query_log

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

# wait for purge view progress (records are deleted actually by purge)
--source include/wait_innodb_all_purged.inc

# secondary index is not merged yet
# | 1,..,11 | 22,..,32 |

--echo # check page merge happens (nothing is expected)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';

select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;


set global innodb_purge_stop_now=ON;
delete from tab1 where a = 32;
set global innodb_purge_run_now=ON;
# wait for purge view progress (records are deleted actually by purge)
--source include/wait_innodb_all_purged.inc

--echo # check page merge happens (MERGE_THRESHOLD=50 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';


set global innodb_purge_stop_now=ON;
delete from tab1 where a = 31;
set global innodb_purge_run_now=ON;
# wait for purge view progress (records are deleted actually by purge)
--source include/wait_innodb_all_purged.inc

--echo # check page merge happens (MERGE_THRESHOLD=45 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';


set global innodb_purge_stop_now=ON;
delete from tab1 where a = 30;
set global innodb_purge_run_now=ON;
# wait for purge view progress (records are deleted actually by purge)
--source include/wait_innodb_all_purged.inc

--echo # check page merge happens (MERGE_THRESHOLD=40 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';

--disable_query_log
# Reset flags
SET GLOBAL innodb_monitor_disable=index_page_merge_attempts;
SET GLOBAL innodb_monitor_disable=index_page_merge_successful;

--disable_warnings
set global innodb_monitor_enable = default;
set global innodb_monitor_disable = default;
set global innodb_monitor_reset = default;
set global innodb_monitor_reset_all = default;
--enable_warnings
--enable_query_log

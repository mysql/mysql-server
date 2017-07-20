#
# Test to cause merge of the pages (by updating to smaller)
# test/tab1 should be created already with innodb_file_per_table=ON
# The definition is intended to be based on
# "create table tab1 (a bigint primary key, b varchar(2048)) engine=InnoDB;"
#

--echo # check MERGE_THRESHOLD
select t.NAME as TABLE_NAME, i.NAME as INDEX_NAME, i.MERGE_THRESHOLD
from INFORMATION_SCHEMA.INNODB_TABLES t, INFORMATION_SCHEMA.INNODB_INDEXES i
where t.TABLE_ID = i.TABLE_ID and t.NAME like 'test/tab1%' ORDER BY i.NAME;

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

# filled 2 leaf pages have been prepared
# | 1,..,7 | 8,..,14 |

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

update tab1 set b='' where a = 12;
update tab1 set b='' where a = 13;
update tab1 set b='' where a = 14;
update tab1 set b='' where a = 5;
update tab1 set b='' where a = 6;
update tab1 set b='' where a = 7;

# not merged yet
# | 1,2,3,4 | 8,9,10,11 |

--echo # check page merge happens (nothing is expected)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';

select PAGE_NUMBER, NUMBER_RECORDS
from INFORMATION_SCHEMA.INNODB_TABLESPACES s1,
INFORMATION_SCHEMA.INNODB_BUFFER_PAGE s2,
INFORMATION_SCHEMA.INNODB_TABLES s3
where s1.SPACE = s2.SPACE AND s1.SPACE = s3.SPACE AND s3.NAME like 'test/tab1%'
and PAGE_TYPE = "INDEX" order by PAGE_NUMBER, NUMBER_RECORDS;

update tab1 set b='' where a = 11;

--echo # check page merge happens (MERGE_THRESHOLD=50 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';


update tab1 set b='' where a = 10;

--echo # check page merge happens (MERGE_THRESHOLD=35 causes merge here)
SELECT name,count_reset FROM information_schema.innodb_metrics
WHERE name like 'index_page_merge_%';


update tab1 set b='' where a = 9;

--echo # check page merge happens (MERGE_THRESHOLD=25 causes merge here)
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

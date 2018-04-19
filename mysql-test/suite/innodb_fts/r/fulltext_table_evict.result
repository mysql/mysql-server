#
# Bug Bug #27304661 MYSQL CRASH DOING SYNC INDEX ]
# [FATAL] INNODB: SEMAPHORE WAIT HAS LASTED > 600
#
CREATE TABLE t1 (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
f1 TEXT(500),
FULLTEXT idx (f1)
) ENGINE=InnoDB;
insert into t1 (f1) values ('aditya'),('sachin'),('aakansha');
# restart
set @save_table_definition_cache=@@global.table_definition_cache;
set @save_table_open_cache=@@global.table_open_cache;
set global table_definition_cache=400;
set global table_open_cache= 50;
SET GLOBAL DEBUG="+d,crash_if_fts_table_is_evicted";
select * from t1;
id	f1
1	aditya
2	sachin
3	aakansha
set @@global.table_definition_cache=@save_table_definition_cache;
set @@global.table_open_cache=@save_table_open_cache;
drop table t1;

CREATE TABLE t(j1 JSON) engine=innodb;
show create table t;
Table	Create Table
t	CREATE TABLE `t` (
  `j1` json DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
start transaction;
# Insert data
SET SESSION DEBUG='+d,crash_end_of_lob_rollback_0';
rollback;
ERROR HY000: Lost connection to MySQL server during query
# restart
select count(*) from t;
count(*)
0
drop table t;

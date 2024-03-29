SET foreign_key_checks=0;
CREATE TABLE t1 (id int, foreign key (id) references t2(id)) ENGINE=INNODB;
CREATE TABLE t2 (id int, foreign key (id) references t1(id)) ENGINE=INNODB;
SET foreign_key_checks=1;
SELECT COUNT(*) FROM information_schema.key_column_usage WHERE REFERENCED_TABLE_NAME in ('t1', 't2')
AND table_schema != 'PERFORMANCE_SCHEMA';
COUNT(*)
2
SET foreign_key_checks=0;
DROP TABLE t1, t2;

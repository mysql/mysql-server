CREATE TABLE t (a INT, b TEXT);
SET DEBUG='+d,alter_table_rollback_new_index';
ALTER TABLE t ADD FULLTEXT INDEX (b(64));
ERROR HY000: Unknown error
SET DEBUG='-d,alter_table_rollback_new_index';
DROP TABLE t;

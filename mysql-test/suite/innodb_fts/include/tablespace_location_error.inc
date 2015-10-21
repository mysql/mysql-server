--let MYSQLD_DATADIR=`SELECT @@datadir`

SET DEBUG = '+d,ib_fts_aux_table_error';
--error 1030
--eval CREATE TABLE t1(a TEXT, b TEXT, c TEXT, d TEXT, FULLTEXT idx(a,b), FULLTEXT idx2(c,d)) $EXTRA_PARAMS
SET DEBUG = '-d,ib_fts_aux_table_error';

--source suite/innodb/include/show_i_s_tables.inc

--echo # list files in datadir/test
--list_files $MYSQLD_DATADIR/test

SET DEBUG = '+d,ib_fts_index_table_error';
--error 1030
--eval CREATE TABLE t1(a TEXT, b TEXT, c TEXT, d TEXT, FULLTEXT idx(a,b), FULLTEXT idx2(c,d)) $EXTRA_PARAMS
SET DEBUG = '-d,ib_fts_index_table_error';

--source suite/innodb/include/show_i_s_tables.inc

--echo # list files in datadir/test
--list_files $MYSQLD_DATADIR/test

--eval CREATE TABLE t1 (id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY, title VARCHAR(200)) ENGINE=InnoDB $EXTRA_PARAMS
--source suite/innodb/include/show_i_s_tables.inc

--echo # list files in datadir/test
--list_files $MYSQLD_DATADIR/test

SET DEBUG = '+d,ib_fts_aux_table_error';
--error 1030
ALTER TABLE t1 ADD FULLTEXT INDEX (title);
SET DEBUG = '-d,ib_fts_aux_table_error';

--source suite/innodb/include/show_i_s_tables.inc
--echo # list files in datadir/test
--let $regexp=/FTS_([0-9a-f_]+)([A-Z0-9_]+)\.i/FTS_AUX_\2.i/
--replace_regex $regexp
--list_files $MYSQLD_DATADIR/test

SET DEBUG = '+d,ib_fts_index_table_error';
--error 1030
ALTER TABLE t1 ADD FULLTEXT INDEX (title);
SET DEBUG = '-d,ib_fts_index_table_error';

--source suite/innodb/include/show_i_s_tables.inc
--echo # list files in datadir/test
--replace_regex $regexp
--list_files $MYSQLD_DATADIR/test
DROP TABLE t1;

--eval CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT fts_idx(a,b), c INT PRIMARY KEY) $EXTRA_PARAMS

INSERT INTO t1 VALUES
        ('MySQL Tutorial','DBMS stands for DataBase', 1)  ,
        ('How To Use MySQL Well','After you went through a', 2),
        ('Optimizing MySQL','In this tutorial we will show', 3);

INSERT INTO t1 VALUES
        ('1001 MySQL Tricks','1. Never run mysqld as root', 4),
        ('MySQL vs. YourSQL','In the following database comparison', 5),
        ('MySQL Security','When configured properly, MySQL', 6);

SELECT c FROM t1 WHERE MATCH (a,b)
        AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);

--source suite/innodb/include/show_i_s_tables.inc
--echo # list files in datadir/test
--replace_regex $regexp
--list_files $MYSQLD_DATADIR/test

SET DEBUG = '+d,ib_fts_aux_table_error';
--error 1030
ALTER TABLE t1 ADD COLUMN pk INT NOT NULL AUTO_INCREMENT, DROP PRIMARY KEY, ADD PRIMARY KEY(pk), ALGORITHM=copy;
SET DEBUG = '-d,ib_fts_aux_table_error';

--source suite/innodb/include/show_i_s_tables.inc
--echo # list files in datadir/test
--replace_regex $regexp
--list_files $MYSQLD_DATADIR/test

SET DEBUG = '+d,ib_fts_index_table_error';
--error 1030
ALTER TABLE t1 ADD COLUMN pk INT NOT NULL AUTO_INCREMENT, DROP PRIMARY KEY, ADD PRIMARY KEY(pk), ALGORITHM=copy;
SET DEBUG = '-d,ib_fts_index_table_error';

--source suite/innodb/include/show_i_s_tables.inc
--echo # list files in datadir/test
--replace_regex $regexp
--list_files $MYSQLD_DATADIR/test

SET DEBUG = '+d,ib_fts_aux_table_error';
ALTER TABLE t1 DROP INDEX fts_idx, ADD FULLTEXT INDEX fts_idx(a,b);
SET DEBUG = '-d,ib_fts_aux_table_error';

--source suite/innodb/include/show_i_s_tables.inc
--echo # list files in datadir/test
--replace_regex $regexp
--list_files $MYSQLD_DATADIR/test

SET DEBUG = '+d,ib_fts_index_table_error';
--error 1030
ALTER TABLE t1 DROP INDEX fts_idx, ADD FULLTEXT INDEX fts_idx(a);
SET DEBUG = '-d,ib_fts_index_table_error';

--source suite/innodb/include/show_i_s_tables.inc
--echo # list files in datadir/test
--replace_regex $regexp
--list_files $MYSQLD_DATADIR/test

DROP TABLE t1;

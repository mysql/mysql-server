--eval CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT fts_idx(a,b), c INT PRIMARY KEY) $CREATE_TABLE_PARAMS
--source suite/innodb/include/show_i_s_tables.inc

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

ALTER  TABLE t1 ADD UNIQUE INDEX idx1(b(20)), ALGORITHM=INPLACE;
--source suite/innodb/include/show_i_s_tables.inc

SELECT c FROM t1 WHERE MATCH (a,b)
        AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);

--echo # Test with ALGORITHM=COPY and with FTS Index
ALTER TABLE t1 ADD COLUMN pk INT NOT NULL AUTO_INCREMENT, DROP PRIMARY KEY, ADD PRIMARY KEY(pk), ALGORITHM=copy;
--source suite/innodb/include/show_i_s_tables.inc

SELECT pk FROM t1 WHERE MATCH (a,b)
        AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);

--echo # Before drop FTS index
--source suite/innodb/include/show_i_s_tables.inc

--echo # Test with DROP FTS Index & ADD FTS Index
ALTER TABLE t1 DROP INDEX fts_idx, ADD FULLTEXT INDEX fts_idx(a,b);
--source suite/innodb/include/show_i_s_tables.inc

SELECT pk FROM t1 WHERE MATCH (a,b)
        AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);

DROP TABLE t1;



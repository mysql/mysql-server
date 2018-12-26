
--eval CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b)) TABLESPACE=$TABLESPACE ROW_FORMAT=$ROW_FORMAT $EXTRA_PARAMS
--source suite/innodb/include/show_i_s_tables.inc

INSERT INTO t1 (a,b) VALUES
        ('MySQL Tutorial','DBMS stands for DataBase')  ,
        ('How To Use MySQL Well','After you went through a'),
        ('Optimizing MySQL','In this tutorial we will show');

INSERT INTO t1 (a,b) VALUES
        ('1001 MySQL Tricks','1. Never run mysqld as root'),
        ('MySQL vs. YourSQL','In the following database comparison'),
        ('MySQL Security','When configured properly, MySQL');

SELECT * FROM t1 WHERE MATCH (a,b)
        AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);

TRUNCATE TABLE t1;
--source suite/innodb/include/show_i_s_tables.inc

INSERT INTO t1 (a,b) VALUES
        ('MySQL Tutorial','DBMS stands for DataBase')  ,
        ('How To Use MySQL Well','After you went through a'),
        ('Optimizing MySQL','In this tutorial we will show');

INSERT INTO t1 (a,b) VALUES
        ('1001 MySQL Tricks','1. Never run mysqld as root'),
        ('MySQL vs. YourSQL','In the following database comparison'),
        ('MySQL Security','When configured properly, MySQL');

SELECT * FROM t1 WHERE MATCH (a,b)
        AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);

DROP TABLE t1;

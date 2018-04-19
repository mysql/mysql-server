#Bug #22445211   GCOLS: SIMPLE DML, FAILING ASSERTION:
#!CURSOR->INDEX->IS_COMMITTED()
#Create and alter table examples for virtual column for full
#column index followed by prefix index.
CREATE TABLE t1(
f1 INT DEFAULT NULL,
f2 CHAR(2) GENERATED ALWAYS AS ('11') VIRTUAL,
f3 INT,
UNIQUE KEY(f1),
UNIQUE KEY(f3,f1),
KEY(f2,f1),
key(f1,f2(1))
)ENGINE=INNODB;
REPLACE INTO t1(f3) VALUES (1),(1);
DROP TABLE t1;
#Create and alter table examples for full column index followed by prefix index.
CREATE TABLE t1(
f1 VARCHAR(100),
f2 char(2),
KEY(f1,f2),
KEY(f1(5)))ENGINE=INNODB;
REPLACE INTO t1(f2) VALUES (1),(1);
ALTER TABLE t1 ADD INDEX(f2,f1);
DROP TABLE t1;
#Create and alter table examples for small prefix index followed by large
#prefix index.
CREATE TABLE t1(
f1 VARCHAR(100),
f2 char(2),
KEY(f1(5),f2),
KEY(f1(10)))ENGINE=INNODB;
REPLACE INTO t1(f2) VALUES (1),(1);
ALTER TABLE t1 ADD INDEX(f2,f1);
DROP TABLE t1;
#Create and alter table examples for prefix index followed by full column
#index.
CREATE TABLE t1(
f1 VARCHAR(100),
f2 char(2),
KEY(f1(5),f2),
KEY(f1))ENGINE=INNODB;
REPLACE INTO t1(f2) VALUES (1),(1);
ALTER TABLE t1 ADD INDEX(f2,f1);
DROP TABLE t1;

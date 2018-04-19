CREATE TABLE t1 (
a	varchar(40),
KEY	a(a)
) ENGINE=InnoDB;
CREATE TABLE t1_fk (
a	varchar(40),
KEY	a(a),
FULLTEXT KEY (a),
CONSTRAINT fk FOREIGN KEY (a) REFERENCES t1 (a) ON UPDATE CASCADE
) ENGINE=InnoDB;
INSERT INTO t1 VALUES('mysql');
INSERT INTO t1_fk VALUES('mysql');
INSERT INTO t1_fk VALUES('mysql');
SELECT * FROM t1_fk;
a
mysql
mysql
SELECT * FROM t1_fk WHERE MATCH(a) AGAINST('mysql');
a
mysql
mysql
UPDATE t1 SET a = 'database' WHERE a = 'mysql';
SELECT * FROM t1_fk;
a
database
database
SELECT * FROM t1_fk WHERE MATCH(a) AGAINST('mysql');
a
SELECT * FROM t1_fk WHERE MATCH(a) AGAINST('database');
a
database
database
DROP TABLE t1_fk;
DROP TABLE t1;

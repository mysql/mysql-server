--echo #
--echo # Bug#36981 - "innodb crash when selecting for update"
--echo #

#
# Test 1: Test based on the reproduction test case for this bug.
#         This query resulted in a crash in InnoDB due to
#         InnoDB changing from using the index which the push condition
#         where for to use the clustered index due to "SELECT ... FOR UPDATE".
#

CREATE TABLE t1 (
  c1 CHAR(1),
  c2 CHAR(10),
  KEY (c1)
);

INSERT INTO t1 VALUES ('3', null);

SELECT * FROM t1 WHERE c1='3' FOR UPDATE;

DROP TABLE t1;

#
# Test 2: Extended test case to test that the correct rows are returned.
#         This test is for ensuring that if InnoDB refuses to accept
#         the pushed index condition it is still evaluated.
#         

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

CREATE TABLE t2 (a INT);
INSERT INTO t2 SELECT A.a + 10*(B.a + 10*C.a) FROM t1 A, t1 B, t1 C;

CREATE TABLE t3 (
  c1 CHAR(10) NOT NULL,
  c2 CHAR(10) NOT NULL,
  c3 CHAR(200) NOT NULL,
  KEY (c1)
);

INSERT INTO t3 
  SELECT CONCAT('c-',1000+t2.a,'=w'), CONCAT('c-',1000+ t2.a,'=w'), 'filler'
  FROM t2;

INSERT INTO t3 
  SELECT CONCAT('c-',1000+t2.a,'=w'), CONCAT('c-',2000+t2.a,'=w'), 'filler-1'
  FROM t2;
 
INSERT INTO t3
  SELECT CONCAT('c-',1000+t2.a,'=w'), CONCAT('c-',3000+t2.a,'=w'), 'filler-2'
  FROM t2;

SELECT c1,c3 FROM t3 WHERE c1 >= 'c-1994=w' and c1 != 'c-1996=w' FOR UPDATE;

DROP TABLE t1,t2,t3;

--echo #
--echo # Bug#43360 - Server crash with a simple multi-table update
--echo #

CREATE TABLE t1 (
  a CHAR(2) NOT NULL PRIMARY KEY, 
  b VARCHAR(20) NOT NULL,
  KEY (b)
);

CREATE TABLE t2 (
  a CHAR(2) NOT NULL PRIMARY KEY,
  b VARCHAR(30) NOT NULL,
  KEY (b)
);

INSERT INTO t1 VALUES 
('AB','MySQL AB'),
('JA','Sun Microsystems'),
('MS','Microsoft'),
('IB','IBM- Inc.'),
('GO','Google Inc.');

INSERT INTO t2 VALUES
('AB','Sweden'),
('JA','USA'),
('MS','United States of America'),
('IB','North America'),
('GO','South America');
 
UPDATE t1,t2 SET t1.b=UPPER(t1.b) WHERE t1.b LIKE 'United%';

SELECT * FROM t1 ORDER BY a;

SELECT * FROM t2 ORDER BY a;

DROP TABLE t1,t2;   

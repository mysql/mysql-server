#
# Bug #47453: InnoDB incorrectly changes TIMESTAMP columns when
#  JOINed during an UPDATE
#
CREATE TABLE t1 (d INT) ENGINE=InnoDB;
CREATE TABLE t2 (a INT, b INT,
c TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6)
ON UPDATE CURRENT_TIMESTAMP(6)) ENGINE=InnoDB;
# set up our data elements
INSERT INTO t1 (d) VALUES (1);
INSERT INTO t2 (a,b) VALUES (1,1);
SELECT c INTO @bug47453 FROM t2;
SELECT c!=@bug47453 FROM t1 JOIN t2 ON d=a;
c!=@bug47453
0
UPDATE t1 JOIN t2 ON d=a SET b=1 WHERE a=1;
SELECT c!=@bug47453 FROM t1 JOIN t2 ON d=a;
c!=@bug47453
0
UPDATE t1 JOIN t2 ON d=a SET b=1;
# should be 0
SELECT c!=@bug47453 FROM t1 JOIN t2 ON d=a;
c!=@bug47453
0
UPDATE t1 JOIN t2 ON d=a SET b=2;
# should not be 0
SELECT c!=@bug47453 FROM t1 JOIN t2 ON d=a;
c!=@bug47453
1
DROP TABLE t1, t2;

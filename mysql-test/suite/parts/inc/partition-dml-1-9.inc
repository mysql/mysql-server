let $DATA_PATH=;
let $INDEX_PATH=;
--source include/have_partition.inc

--echo # Additional tests for WL#5217 by QA, testplan 1.1

let $TABLENAME= t1;
--source suite/parts/inc/partition-dml_cr_tab.inc
--source suite/parts/inc/partition-dml_ins_tab.inc
let $TABLENAME= t2;
--source suite/parts/inc/partition-dml_cr_tab.inc
--source suite/parts/inc/partition-dml_ins_tab.inc

--enable_connect_log

GRANT ALL PRIVILEGES ON test.* TO test_user_1 IDENTIFIED BY 'testpw';

connect (session1, localhost, test_user_1,'testpw',test);
CREATE VIEW v1 AS
SELECT t1.a, t2.b FROM t1 PARTITION (`p0-29`), t2 
WHERE t1.a = t2.a;
--sorted_result
SELECT * FROM v1;
--sorted_result
SELECT t1.a, t2.b FROM t1 PARTITION (`p0-29`), t2
WHERE t1.a = t2.a;

--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
--sorted_result
SELECT * FROM t2 PARTITION (`p30-299`);
--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM t1 PARTITION (`p0-29`), 
t2 PARTITION (`p30-299`);

--sorted_result
SELECT * FROM t1 PARTITION (subp3);
--sorted_result
SELECT * FROM t2 PARTITION (subp6);
--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM t1 PARTITION (subp3), 
t2 PARTITION (subp6);

--sorted_result
SELECT * FROM t1 PARTITION (subp3,subp5);
--sorted_result
SELECT * FROM t2 PARTITION (subp6,subp8);
--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM t1 PARTITION (subp3,subp5), 
t2 PARTITION (subp6,subp8);

DELETE FROM t2 PARTITION (`p30-299`);

--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
--sorted_result
SELECT * FROM t2  PARTITION (`p30-299`);
--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM t1 PARTITION (`p0-29`), 
t2 PARTITION (`p30-299`);

--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM 
t1 PARTITION (`p0-29`,`p30-299`) 
LEFT JOIN
t2 
USING (a,b);

--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM 
t1 
LEFT JOIN
t2 PARTITION (`p0-29`,`p30-299`) 
USING (a,b);

--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM 
t1 PARTITION (`p0-29`,`p30-299`) 
RIGHT JOIN
t2 
USING (a,b);

--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM 
t1 
RIGHT JOIN
t2 PARTITION (`p0-29`,`p30-299`) 
USING (a,b);

DELETE FROM t1 PARTITION (`p0-29`);

--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
--sorted_result
SELECT * FROM t2  PARTITION (`p30-299`);
--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM t1 PARTITION (`p0-29`), 
t2 PARTITION (`p30-299`);

INSERT INTO t1 VALUES (1,'t1-p0-29-ins');

--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
--sorted_result
SELECT * FROM t2  PARTITION (`p30-299`);
--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM t1 PARTITION (`p0-29`), 
t2 PARTITION (`p30-299`);

INSERT INTO t2 VALUES (32,'t2-p30-299-ins');

--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
--sorted_result
SELECT * FROM t2  PARTITION (`p30-299`);
--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM t1 PARTITION (`p0-29`), 
t2 PARTITION (`p30-299`);

DELETE FROM t1 PARTITION (`p30-299`);
INSERT INTO t1 VALUES (31,'t1-p30-299-ins');
DELETE FROM t2 PARTITION (`p0-29`);
INSERT INTO t2 VALUES (2,'t2-p0-29-ins');

--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`,`p30-299`);
--sorted_result
SELECT * FROM t2  PARTITION (`p0-29`,`p30-299`);
--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM t1 PARTITION (`p0-29`,`p30-299`), 
t2 PARTITION (`p0-29`,`p30-299`);

--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM 
t1 PARTITION (`p0-29`,`p30-299`) 
JOIN
t2 PARTITION (`p0-29`,`p30-299`);

--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM 
t1 PARTITION (`p0-29`,`p30-299`) 
INNER JOIN
t2 PARTITION (`p0-29`,`p30-299`);

--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM 
t1 PARTITION (`p0-29`,`p30-299`) 
INNER JOIN
t2 PARTITION (`p0-29`,`p30-299`)
USING (a,b);

--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM 
t1 PARTITION (`p0-29`,`p30-299`) 
NATURAL JOIN
t2 PARTITION (`p0-29`,`p30-299`);

--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM 
t1 PARTITION (`p0-29`,`p30-299`) 
LEFT JOIN
t2 PARTITION (`p0-29`,`p30-299`)
USING (a,b);

--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM 
t1 PARTITION (`p0-29`,`p30-299`) 
RIGHT JOIN
t2 PARTITION (`p0-29`,`p30-299`)
USING (a,b);

INSERT INTO t1 VALUES (32,'t2-p30-299-ins');
INSERT INTO t2 VALUES (1,'t1-p0-29-ins');

--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM 
t1 PARTITION (`p0-29`,`p30-299`) 
INNER JOIN
t2 PARTITION (`p0-29`,`p30-299`)
USING (a,b);

--sorted_result
SELECT t1.a AS t1a,t2.a AS t2a,t1.b AS t1b,t2.b AS t2b FROM 
t1 PARTITION (`p0-29`,`p30-299`) 
NATURAL JOIN
t2 PARTITION (`p0-29`,`p30-299`);

disconnect session1;
connection default;
DROP VIEW v1;
DROP TABLE t1, t2;
DROP USER test_user_1;


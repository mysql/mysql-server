include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
DROP TRIGGER test.t2_ai;
DROP TABLE IF EXISTS test.t1;
DROP TABLE IF EXISTS test.t2;
DROP TABLE IF EXISTS test.t3;
CREATE TABLE test.t2 (value CHAR(30),domain_id INT, mailaccount_id INT, program CHAR(30),keey CHAR(30),PRIMARY KEY(domain_id));
CREATE TABLE test.t3 (value CHAR(30),domain_id INT, mailaccount_id INT, program CHAR(30),keey CHAR(30),PRIMARY KEY(domain_id));
CREATE TABLE test.t1 (id INT,domain CHAR(30),PRIMARY KEY(id));
CREATE TRIGGER test.t2_ai AFTER INSERT ON test.t2 FOR EACH ROW UPDATE test.t3 ms, test.t1 d  SET ms.value='No' WHERE ms.domain_id = (SELECT max(id) FROM test.t1 WHERE domain='example.com') AND ms.mailaccount_id IS NULL  AND ms.program='spamfilter' AND ms.keey='scan_incoming'|
INSERT INTO test.t1 VALUES (1, 'example.com'),(2, 'mysql.com'),(3, 'earthmotherwear.com'), (4, 'yahoo.com'),(5, 'example.com');
SELECT * FROM test.t1 ORDER BY id;
id	domain
1	example.com
2	mysql.com
3	earthmotherwear.com
4	yahoo.com
5	example.com
include/sync_slave_sql_with_master.inc
SELECT * FROM test.t1 ORDER BY id;
id	domain
1	example.com
2	mysql.com
3	earthmotherwear.com
4	yahoo.com
5	example.com
INSERT INTO test.t3 VALUES ('Yes', 5, NULL, 'spamfilter','scan_incoming');
INSERT INTO test.t3 VALUES ('Yes', 1, NULL, 'spamfilter','scan_incoming');
INSERT INTO test.t2 VALUES ('Yes', 1, NULL, 'spamfilter','scan_incoming');
select * from test.t2;
value	domain_id	mailaccount_id	program	keey
Yes	1	NULL	spamfilter	scan_incoming
select * from test.t3 order by value;
value	domain_id	mailaccount_id	program	keey
No	5	NULL	spamfilter	scan_incoming
Yes	1	NULL	spamfilter	scan_incoming
include/sync_slave_sql_with_master.inc
select * from test.t2;
value	domain_id	mailaccount_id	program	keey
Yes	1	NULL	spamfilter	scan_incoming
select * from test.t3 order by value;
value	domain_id	mailaccount_id	program	keey
No	5	NULL	spamfilter	scan_incoming
Yes	1	NULL	spamfilter	scan_incoming
DELETE FROM test.t1 WHERE id = 1;
SELECT * FROM test.t1 ORDER BY id;
id	domain
2	mysql.com
3	earthmotherwear.com
4	yahoo.com
5	example.com
SELECT * FROM test.t1 ORDER BY id;
id	domain
2	mysql.com
3	earthmotherwear.com
4	yahoo.com
5	example.com
include/sync_slave_sql_with_master.inc
SELECT * FROM test.t1 ORDER BY id;
id	domain
2	mysql.com
3	earthmotherwear.com
4	yahoo.com
5	example.com
DROP TRIGGER test.t2_ai;
DROP TABLE test.t1;
DROP TABLE test.t2;
DROP TABLE test.t3;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc

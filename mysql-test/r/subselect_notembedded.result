purge master logs before (select adddate(current_timestamp(), interval -4 day));
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '(select adddate(current_timestamp(), interval -4 day))' at line 1
purge master logs before adddate(current_timestamp(), interval -4 day);
create table t1(a int,b int,key(a),key(b));
insert into t1(a,b) values (1,2),(2,1),(2,3),(3,4),(5,4),(5,5),
(6,7),(7,4),(5,3);
5
4
3
2
1
26
25
24
23
22
21
20
19
18
17
16
15
14
13
12
11
10
9
8
7
6
5
4
3
2
1
drop table t1;
End of 5.1 tests
CREATE TABLE t1 (f1 INT NOT NULL);
CREATE VIEW v1 (a) AS SELECT f1 IN (SELECT f1 FROM t1) FROM t1;
SELECT * FROM v1;
a
drop view v1;
drop table t1;

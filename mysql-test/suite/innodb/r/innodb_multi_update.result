CREATE TABLE bug38999_1 (a int not null primary key, b int not null, key (b)) engine=innodb;
CREATE TABLE bug38999_2 (a int not null primary key, b int not null, key (b)) engine=innodb;
INSERT INTO bug38999_1 values (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9),(10,10),(11,11),(12,12);
INSERT INTO bug38999_2 values (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9);
update bug38999_1,bug38999_2 set bug38999_1.a=bug38999_1.a+100;
select * from bug38999_1;
a	b
101	1
102	2
103	3
104	4
105	5
106	6
107	7
108	8
109	9
110	10
111	11
112	12
update bug38999_1,bug38999_2 set bug38999_1.a=bug38999_1.a+100 where bug38999_1.a=101;
select * from bug38999_1;
a	b
102	2
103	3
104	4
105	5
106	6
107	7
108	8
109	9
110	10
111	11
112	12
201	1
update bug38999_1,bug38999_2 set bug38999_1.b=bug38999_1.b+10 where bug38999_1.b=2;
select * from bug38999_1;
a	b
102	12
103	3
104	4
105	5
106	6
107	7
108	8
109	9
110	10
111	11
112	12
201	1
update bug38999_1 force index (b) straight_join bug38999_2
set bug38999_1.b=bug38999_1.b+2,
bug38999_2.b=bug38999_1.b+10
where bug38999_1.b between 3 and 5 and bug38999_1.a=bug38999_2.a+100;
select * from bug38999_1;
a	b
102	12
103	5
104	6
105	7
106	6
107	7
108	8
109	9
110	10
111	11
112	12
201	1
select * from bug38999_2;
a	b
1	1
2	2
3	13
4	14
5	15
6	6
7	7
8	8
9	9
drop table bug38999_1,bug38999_2;
#
# Bug#54475 improper error handling causes cascading crashing failures in innodb/ndb
#
CREATE TABLE t1(f1 INT) ENGINE=INNODB;
INSERT INTO t1 VALUES(1);
UPDATE (SELECT ((SELECT 1 FROM t1), 1) FROM t1 WHERE (SELECT 1 FROM t1)) x, (SELECT 1) AS d SET d.f1 = 1;
ERROR 21000: Operand should contain 1 column(s)
DROP TABLE t1;

CREATE USER test_2@localhost;
send
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
connect (test_2,localhost,test_2,,test,MASTER_MYPORT,MASTER_MYSOCK);
SELECT * FROM t_10;
a	b
2	Two
4	Four
6	Six
8	Eight
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
a	b
1	One
3	Three
5	Five
9	Nine
INSERT INTO tp VALUES (7,"Seven");
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
a	b
1	One
3	Three
5	Five
7	Seven
9	Nine
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
connection default;
reap;
SELECT * FROM t_10;
a	b
1	One
3	Three
5	Five
7	Seven
9	Nine
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
a	b
2	Two
4	Four
6	Six
8	Eight
connection test_2;
SELECT * FROM t_10;
a	b
1	One
3	Three
5	Five
7	Seven
9	Nine
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
a	b
2	Two
4	Four
6	Six
8	Eight
disconnect test_2;
connection default;
SELECT * FROM t_10;
a	b
1	One
3	Three
5	Five
7	Seven
9	Nine
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
a	b
2	Two
4	Four
6	Six
8	Eight
DROP TABLE IF EXISTS t_10;
DROP TABLE IF EXISTS t_100;
DROP TABLE IF EXISTS t_1000;
DROP TABLE IF EXISTS tp;
DROP TABLE IF EXISTS tsp;
DROP TABLE IF EXISTS tsp_00;
DROP TABLE IF EXISTS tsp_01;
DROP TABLE IF EXISTS tsp_02;
DROP TABLE IF EXISTS tsp_03;
DROP TABLE IF EXISTS tsp_04;
DROP TABLE IF EXISTS t_empty;
DROP TABLE IF EXISTS t_null;
DROP USER test_2@localhost;

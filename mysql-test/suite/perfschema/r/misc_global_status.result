CREATE USER user1@localhost;
CREATE USER user2@localhost;
CREATE USER user3@localhost;
grant ALL on *.* to user1@localhost;
grant ALL on *.* to user2@localhost;
grant ALL on *.* to user3@localhost;
TRUNCATE TABLE performance_schema.accounts;
FLUSH PRIVILEGES;
CREATE TABLE test.t_range(a int, b int, PRIMARY KEY(a));
INSERT INTO test.t_range values (1, 1), (2,2), (3, 3), (4, 4), (5, 5);
INSERT INTO test.t_range values (6, 6), (7,7), (8, 8), (9, 9), (10, 10);
FLUSH STATUS;
SELECT * from test.t_range where (a >= 3) AND (a <= 5);
a	b
3	3
4	4
5	5
SELECT * from performance_schema.session_status
WHERE VARIABLE_NAME = 'Select_range';
VARIABLE_NAME	VARIABLE_VALUE
Select_range	1
VARIABLE_NAME	DELTA
Select_range	1
SELECT * from test.t_range where (a >= 3) AND (a <= 5);
a	b
3	3
4	4
5	5
SELECT * from performance_schema.session_status
WHERE VARIABLE_NAME = 'Select_range';
VARIABLE_NAME	VARIABLE_VALUE
Select_range	1
VARIABLE_NAME	DELTA
Select_range	2
SELECT * from test.t_range where (a >= 3) AND (a <= 5);
a	b
3	3
4	4
5	5
SELECT * from test.t_range where (a >= 4) AND (a <= 6);
a	b
4	4
5	5
6	6
SELECT * from performance_schema.session_status
WHERE VARIABLE_NAME = 'Select_range';
VARIABLE_NAME	VARIABLE_VALUE
Select_range	2
VARIABLE_NAME	DELTA
Select_range	4
SELECT * from test.t_range where (a >= 3) AND (a <= 5);
a	b
3	3
4	4
5	5
SELECT * from test.t_range where (a >= 4) AND (a <= 6);
a	b
4	4
5	5
6	6
SELECT * from test.t_range where (a >= 5) AND (a <= 7);
a	b
5	5
6	6
7	7
SELECT * from performance_schema.session_status
WHERE VARIABLE_NAME = 'Select_range';
VARIABLE_NAME	VARIABLE_VALUE
Select_range	3
VARIABLE_NAME	DELTA
Select_range	7
VARIABLE_NAME	DELTA
Select_range	7
SELECT `USER`, `HOST`, VARIABLE_NAME, VARIABLE_VALUE
FROM performance_schema.status_by_account WHERE VARIABLE_NAME = 'Select_range'
  AND `USER` LIKE 'user%'
  ORDER BY `USER`;
USER	HOST	VARIABLE_NAME	VARIABLE_VALUE
user1	localhost	Select_range	1
user2	localhost	Select_range	2
user3	localhost	Select_range	3
VARIABLE_NAME	DELTA
Select_range	7
TRUNCATE TABLE performance_schema.accounts;
VARIABLE_NAME	DELTA
Select_range	7
VARIABLE_NAME	DELTA
Select_range	7
TRUNCATE TABLE performance_schema.accounts;
VARIABLE_NAME	DELTA
Select_range	7
DROP TABLE test.t_range;
REVOKE ALL PRIVILEGES, GRANT OPTION FROM user1@localhost;
REVOKE ALL PRIVILEGES, GRANT OPTION FROM user2@localhost;
REVOKE ALL PRIVILEGES, GRANT OPTION FROM user3@localhost;
DROP USER user1@localhost;
DROP USER user2@localhost;
DROP USER user3@localhost;
FLUSH PRIVILEGES;

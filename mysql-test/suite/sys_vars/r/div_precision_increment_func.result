drop table if exists t1;
## Creating new table ##
CREATE TABLE t1
(
id INT NOT NULL auto_increment,
PRIMARY KEY (id),
name VARCHAR(30),
salary INT
);
'#--------------------FN_DYNVARS_027_01-------------------------#'
## Setting initial session value of variable to 3 ##
SET @@session.div_precision_increment = 3;
## Inserting some rows in table ##
INSERT into t1(name, salary) values('Record_1', 100011);
INSERT into t1(name, salary) values('Record_2', 501);
INSERT into t1(name, salary) values('Record_3', 210);
SELECT name, salary, ((salary * 2.5)/1000) AS INCOME from t1;
name	salary	INCOME
Record_1	100011	250.0275
Record_2	501	1.2525
Record_3	210	0.5250
## Verifying variable's behavior with direct division ##
SELECT 1/7;
1/7
0.143
'#--------------------FN_DYNVARS_027_02-------------------------#'
drop table if exists t1;
## Creating new table ##
CREATE TABLE t1
(
id INT NOT NULL auto_increment,
PRIMARY KEY (id),
name VARCHAR(30),
salary INT,
income_tax FLOAT
);
## Creating new connection test_con1 ##
## Setting global & session scope value of variable ##
SET @@global.div_precision_increment = 2;
SET @@session.div_precision_increment = 1;
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
2
SELECT @@session.div_precision_increment;
@@session.div_precision_increment
1
## Inserting some data and verifying behavior of variable ##
INSERT into t1(name, salary, income_tax) values('Record_1', 100011, 100011*2.5/1000);
INSERT into t1(name, salary, income_tax) values('Record_2', 501, 501*2.5/1000);
INSERT into t1(name, salary, income_tax) values('Record_3', 210, 210*2.5/1000);
SELECT * from t1;
id	name	salary	income_tax
1	Record_1	100011	250.027
2	Record_2	501	1.2525
3	Record_3	210	0.525
## Creating new connection ## 
## Verifying session & global value of variable ##
SELECT @@global.div_precision_increment = 2;
@@global.div_precision_increment = 2
1
SELECT @@session.div_precision_increment = 2;
@@session.div_precision_increment = 2
1
## Verifying behavior of variable by inserting some rows in table ##
INSERT into t1(name, salary, income_tax) values('Record_4', 100011, 100011*2.5/1000);
INSERT into t1(name, salary, income_tax) values('Record_5', 501, 501*2.5/1000);
INSERT into t1(name, salary, income_tax) values('Record_6', 210, 210*2.5/1000);
SELECT * from t1;
id	name	salary	income_tax
1	Record_1	100011	250.027
2	Record_2	501	1.2525
3	Record_3	210	0.525
4	Record_4	100011	250.027
5	Record_5	501	1.2525
6	Record_6	210	0.525
## Dropping table t1 ##
drop table t1;
## Disconnection both the connections ##
SET @@global.div_precision_increment = 4;

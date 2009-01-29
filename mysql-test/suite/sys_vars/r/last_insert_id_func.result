drop table if exists t1;
## Creating new table ##
CREATE TABLE t1
(
id INT NOT NULL auto_increment,
PRIMARY KEY (id),
name VARCHAR(30)
) ENGINE = INNODB;
'#--------------------FN_DYNVARS_059_01-------------------------#'
## Verifying initial value of ## 
SELECT @@session.last_insert_id;
@@session.last_insert_id
0
## Inserting records in table t1 ##
INSERT into t1(name) values('Record_1');
INSERT into t1(name) values('Record_2');
SELECT * from t1;
id	name
1	Record_1
2	Record_2
## Verifying value of variable after inserting some rows ## 
SELECT @@session.last_insert_id = 2;
@@session.last_insert_id = 2
1
'#--------------------FN_DYNVARS_059_02-------------------------#'
## Creating & connecting to new connection test_con1 ##
SET @@autocommit = 0;
## Verifying initial value of variable in new connection ## 
SELECT @@session.last_insert_id;
@@session.last_insert_id
0
## Inserting rows in table t1 ## 
START TRANSACTION;
INSERT into t1(name) values('Record_3');
INSERT into t1(name) values('Record_4');
INSERT into t1(name) values('Record_5');
## Verifying value of variable without committing rows ## 
SELECT @@session.last_insert_id;
@@session.last_insert_id
5
'#--------------------FN_DYNVARS_059_03-------------------------#'
## Creating & connecting to new connection test_con2 ##
## Inserting values through new connection ##
INSERT into t1(name) values('Record_6');
INSERT into t1(name) values('Record_7');
SELECT * from t1;
id	name
1	Record_1
2	Record_2
6	Record_6
7	Record_7
## Verifying value of variable in second connection ## 
SELECT @@last_insert_id;
@@last_insert_id
7
'#--------------------FN_DYNVARS_059_04-------------------------#'
## Switching to test_con1 ##
## Verifying all records in table & value of variable ##
SELECT * from t1;
id	name
1	Record_1
2	Record_2
3	Record_3
4	Record_4
5	Record_5
6	Record_6
7	Record_7
SELECT @@session.last_insert_id;
@@session.last_insert_id
5
## Commiting records in table ##
COMMIT;
SELECT @@session.last_insert_id;
@@session.last_insert_id
5
## Switching to test_con2 & verifying value of variable in it ## 
SELECT @@session.last_insert_id;
@@session.last_insert_id
7
'#--------------------FN_DYNVARS_059_05-------------------------#'
## Setting value of variable ## 
SET @@session.last_insert_id = 100;
SELECT @@session.last_insert_id;
@@session.last_insert_id
100
## Inserting new record and verifying variable's effect on it ##
INSERT into t1(name) values('Record_8');
SELECT @@session.last_insert_id;
@@session.last_insert_id
8
## Dropping table t1 ##
drop table t1;
## Disconnecting both the connections ##

drop table if exists t1;
## Creating new table ##
CREATE TABLE t1
(
id INT NOT NULL auto_increment,
PRIMARY KEY (id),
name varchar(30)
) ENGINE = INNODB;
'#--------------------FN_DYNVARS_003_01-------------------------#'
## Setting variable's value to 0 i.e false ##
SET @@autocommit = 0;
'#--------------------FN_DYNVARS_003_02-------------------------#'
## Creating new connection ## 
## Checking value of variable after opening new connection ##
SELECT @@autocommit;
@@autocommit
1
## Setting value of variable to zero and inserting some rows ## 
SET @@autocommit = 0;
INSERT into t1(name) values('Record_1');
INSERT into t1(name) values('Record_2');
SELECT * from t1;
id	name
1	Record_1
2	Record_2
## Creating another connection and verifying records in table ## 
## New Connection test_con2 ##
SELECT * from t1;
id	name
'#--------------------FN_DYNVARS_003_03-------------------------#'
## Verifying behavior of variable by commiting rows in test_con1 ##
## Connecting with connection # 01 ## 
SELECT * from t1;
id	name
1	Record_1
2	Record_2
COMMIT;
## New Connection test_con2 ##
## Now verifying records in table from connection # 02 ## 
SELECT * from t1;
id	name
1	Record_1
2	Record_2
'#--------------------FN_DYNVARS_003_04-------------------------#'
## Connecting to connection # 01 ##
SELECT * from t1;
id	name
1	Record_1
2	Record_2
## Updating value of first row ##
UPDATE t1 set name = 'Record_12' where name = 'Record_1';
SELECT * from t1;
id	name
1	Record_12
2	Record_2
## Connecting to connecting # 02 and verifying effect of update query ##
SELECT * from t1;
id	name
1	Record_1
2	Record_2
## Now connecting with connection # 01 and using ROLLBACK after it ##
ROLLBACK;
SELECT * from t1;
id	name
1	Record_1
2	Record_2
'#--------------------FN_DYNVARS_003_05-------------------------#'
## Connecting with connection # 01 ## 
INSERT into t1(name) values('Record_3');
## Connection test_con2 ##
## Now verifying records in table from connection # 02 and changing value  ## 
## of autocommit to true ## 
SELECT * from t1;
id	name
1	Record_1
2	Record_2
SET @@autocommit = 1;
INSERT into t1(name) values('Record_4');
INSERT into t1(name) values('Record_5');
SELECT * from t1;
id	name
1	Record_1
2	Record_2
4	Record_4
5	Record_5
## Connecting with connection # 01 and inserting few records ## 
SELECT * from t1;
id	name
1	Record_1
2	Record_2
3	Record_3
INSERT into t1(name) values('Record_6');
SELECT * from t1;
id	name
1	Record_1
2	Record_2
3	Record_3
6	Record_6
## Now verifying the effect of these new records in second connection ## 
SELECT * from t1;
id	name
1	Record_1
2	Record_2
4	Record_4
5	Record_5
## Commit changes
COMMIT;
## Dropping table t1 ##
DROP table t1;
## Disconnecting both connections ##

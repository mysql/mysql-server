drop table if exists t1;
drop table if exists t2;
## Creating new table t1 ##
CREATE TABLE t1
(
id INT NOT NULL auto_increment,
PRIMARY KEY (id),
name VARCHAR(30)
) ENGINE = INNODB;
## Creating another new table t2 ##
CREATE TABLE t2
(
id INT NOT NULL auto_increment,
PRIMARY KEY (id),
name VARCHAR(30)
) ENGINE = INNODB;
'#--------------------FN_DYNVARS_035_01-------------------------#'
## It should be zero ## 
SELECT @@identity = 0;
@@identity = 0
1
## Creating and connecting with new connection test_con1 ##
SET @@autocommit = 0;
## Inserting rows in table t1 ## 
INSERT into t1(name) values('Record_1');
INSERT into t1(name) values('Record_2');
INSERT into t1(name) values('Record_3');
## Verifying total values in t1 ## 
SELECT @@identity from t1;
@@identity
3
3
3
## Now inserting some data in table t2 ## 
INSERT into t2(name) values('Record_1');
## Verifying total values in t2 ## 
SELECT @@identity from t2;
@@identity
1
'#--------------------FN_DYNVARS_035_02-------------------------#'
## Creating and connecting with new connection test_con2 ##
SELECT * from t1;
id	name
## Verifying total values in t1 ## 
SELECT @@identity from t1;
@@identity
## Verifying total values in t2 ## 
SELECT @@identity from t2;
@@identity
## Inserting some more records in table t1 ##
INSERT into t1(name) values('Record_1_1');
INSERT into t1(name) values('Record_1_2');
## Verifying total values in t1 ## 
SELECT @@identity from t1;
@@identity
5
5
## Inserting row in table t2 ##
INSERT into t2(name) values('Record_1_3');
## Verifying total values in t2 ## 
SELECT @@identity from t2;
@@identity
2
'#--------------------FN_DYNVARS_035_03-------------------------#'
## Switching to connection test_con1 ##
## Commiting rows added in test_con1 ##
COMMIT;
## Verifying records in both tables ##
SELECT * from t1;
id	name
1	Record_1
2	Record_2
3	Record_3
4	Record_1_1
5	Record_1_2
SELECT * from t2;
id	name
1	Record_1
2	Record_1_3
## Verifying total values in t1 after commiting data ## 
SELECT @@identity from t1;
@@identity
1
1
1
1
1
## Verifying total values in t2 after commiting data ## 
SELECT @@identity from t2;
@@identity
1
1
INSERT into t1(name) values('Record_4');
## Now verifying value of variable after inserting 1 row in this connection ## 
SELECT @@identity from t1;
@@identity
6
6
6
6
6
6
## Dropping tables t1 & t2 ##
drop table t1, t2;
## Disconnecting both the connections ##

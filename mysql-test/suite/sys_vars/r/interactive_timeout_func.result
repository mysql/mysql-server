drop table if exists t1;
## Creating new table t1 ##
CREATE TABLE t1
(
id INT NOT NULL auto_increment,
PRIMARY KEY (id),
name VARCHAR(30)
);
'#--------------------FN_DYNVARS_052_01-------------------------#'
## Setting initial value of variable to 1 ##
SET @@global.interactive_timeout = 1;
## Creating new connection test_con1 ##
## Inserting record in table ##
INSERT into t1(name) values('Record_1');
## Setting session value of interactive_timeout ##
SET @@session.interactive_timeout = 1;
## Verifying values of variable ##
SELECT @@session.interactive_timeout;
@@session.interactive_timeout
1
SELECT @@global.interactive_timeout;
@@global.interactive_timeout
1
connection default;
## Using sleep to check timeout ##
sleep 2;
connection test_con1;
SELECT * from t1;
id	name
1	Record_1
INSERT into t1(name) values('Record_2');
connection default;
disconnect test_con1;
DROP TABLE t1;
SET @@global.interactive_timeout= 28800;

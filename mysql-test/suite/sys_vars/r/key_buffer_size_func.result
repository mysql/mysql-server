DROP TABLE IF EXISTS t1;
## Creating new table t1 ##
CREATE TABLE t1
(
id INT NOT NULL AUTO_INCREMENT,
PRIMARY KEY (id),
rollno INT NOT NULL,
name VARCHAR(30)
);
SET @start_value= @@global.key_buffer_size;
FLUSH STATUS;
'#--------------------FN_DYNVARS_055_01-------------------------#'
## Setting initial value of variable to 131072 ##
SET @@global.key_buffer_size = 131072;
## Creating two new connections ##
'#--------------------FN_DYNVARS_055_02-------------------------#'
## Connecting with connection test_con1 ##
SELECT @@global.key_buffer_size;
@@global.key_buffer_size
131072
## Inserting some rows in table ##
INSERT INTO t1(rollno, name) VALUES(1, 'Record_1');
INSERT INTO t1(rollno, name) VALUES(2, 'Record_2');
INSERT INTO t1(rollno, name) VALUES(1, 'Record_3');
INSERT INTO t1(rollno, name) VALUES(3, 'Record_4');
INSERT INTO t1(rollno, name) VALUES(1, 'Record_5');
INSERT INTO t1(rollno, name) VALUES(3, 'Record_6');
INSERT INTO t1(rollno, name) VALUES(4, 'Record_7');
INSERT INTO t1(rollno, name) VALUES(4, 'Record_8');
INSERT INTO t1(rollno, name) VALUES(4, 'Record_9');
INSERT INTO t1(rollno, name) VALUES(4, 'Record_10');
## Key_reads must be zero (no disk access) ##
show status like 'Key_reads';
Variable_name	Value
Key_reads	0
## Switching to connection test_con2 ##
## Key_reads must be zero (no disk access) ##
show status like 'Key_reads';
Variable_name	Value
Key_reads	0
## Connecting with connection test_con1 ##
## Inserting some rows in table ##
INSERT INTO t1(rollno, name) VALUES(5, 'Record_11');
INSERT INTO t1(rollno, name) VALUES(6, 'Record_12');
INSERT INTO t1(rollno, name) VALUES(5, 'Record_13');
INSERT INTO t1(rollno, name) VALUES(7, 'Record_14');
INSERT INTO t1(rollno, name) VALUES(5, 'Record_15');
INSERT INTO t1(rollno, name) VALUES(7, 'Record_16');
INSERT INTO t1(rollno, name) VALUES(8, 'Record_17');
INSERT INTO t1(rollno, name) VALUES(8, 'Record_18');
INSERT INTO t1(rollno, name) VALUES(8, 'Record_19');
INSERT INTO t1(rollno, name) VALUES(8, 'Record_20');
## Key_reads must be zero (no disk access) ##
show status like 'Key_reads';
Variable_name	Value
Key_reads	0
## Dropping table ##
DROP TABLE IF EXISTS t1;
## Disconnecting both the connections ##
SET @@global.key_buffer_size= @start_value;

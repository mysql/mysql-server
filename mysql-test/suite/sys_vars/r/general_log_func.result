drop table if exists t1;
## Creating new table ##
CREATE TABLE t1
(
id INT NOT NULL auto_increment,
PRIMARY KEY (id),
name VARCHAR(30)
);
'#--------------------FN_DYNVARS_018_01-------------------------#'
## Setting initial value of variable to OFF ##
SET @@global.general_log = OFF;
SELECT @@general_log;
@@general_log
0
flush logs;
SET @@global.general_log = ON;
flush logs;
SET @@global.general_log = OFF;
## Inserting some Records & Verifying output in log ##
INSERT into t1(name) values('Record_1');
INSERT into t1(name) values('Record_2');
## There should be no difference, case should pass ##
'#--------------------FN_DYNVARS_018_01-------------------------#'
## Setting initial value of variable to OFF ##
SET @@global.general_log = ON;
SELECT @@general_log;
@@general_log
1
## Inserting some Records & Verifying output in log ##
INSERT into t1(name) values('Record_3');
INSERT into t1(name) values('Record_4');
## old log is a proper prefix of the new log ##
SET @orig_file= load_file('MYSQLD_LOGFILE.orig');
SET @copy_file= load_file('MYSQLD_LOGFILE.copy');
SELECT @orig_file > @copy_file, left(@orig_file, length(@copy_file)) = @copy_file;
@orig_file > @copy_file	left(@orig_file, length(@copy_file)) = @copy_file
1	1
## Dropping tables ##
DROP TABLE t1;

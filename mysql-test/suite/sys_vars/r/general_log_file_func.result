DROP TABLE IF EXISTS t1;
## Creating new table ##
CREATE TABLE t1
(
id INT NOT NULL AUTO_INCREMENT,
PRIMARY KEY (id),
name VARCHAR(30)
);
'#--------------------FN_DYNVARS_018_01-------------------------#'
SELECT @@general_log_file;
@@general_log_file
mysql-test.log
INSERT INTO t1(name) VALUES('Record_1');
INSERT INTO t1(name) VALUES('Record_2');
INSERT INTO t1(name) VALUES('Record_3');
INSERT INTO t1(name) VALUES('Record_4');
## Verifying general log file ##
## Dropping table ##
DROP TABLE t1;

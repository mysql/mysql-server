drop table if exists t1;
'#--------------------FN_DYNVARS_063_01-------------------------#'
SET @start_value= @@global.log_bin_trust_function_creators;
## Creating new user tt ##
CREATE USER tt@localhost;
## Setting value of variable to 0 ##
SET @@global.log_bin_trust_function_creators = 0;
## Creating new table t2 ##
CREATE TABLE t2 (a INT);
## Creating & connecting with new connection test_con1 ##
SELECT @@log_bin_trust_function_creators;
@@log_bin_trust_function_creators
0
##  Creating new function f1 fails because no DETERMINISTIC ###
CREATE FUNCTION f1(a INT) RETURNS INT
BEGIN
IF (a < 3) THEN
INSERT INTO t2 VALUES (a);
END IF;
RETURN 1;
END|
ERROR HY000: This function has none of DETERMINISTIC, NO SQL, or READS SQL DATA in its declaration and binary logging is enabled (you *might* want to use the less safe log_bin_trust_function_creators variable)
## Creating new function f1 fails because non-super user ##
CREATE FUNCTION f1(a INT) RETURNS INT DETERMINISTIC
BEGIN
IF (a < 3) THEN
INSERT INTO t2 VALUES (a);
END IF;
RETURN 1;
END|
ERROR HY000: You do not have the SUPER privilege and binary logging is enabled (you *might* want to use the less safe log_bin_trust_function_creators variable)
## Creating new function f1 succeeds ##
CREATE FUNCTION f1(a INT) RETURNS INT DETERMINISTIC
BEGIN
IF (a < 3) THEN
INSERT INTO t2 VALUES (a);
END IF;
RETURN 1;
END|
## Creating new table t1 ##
CREATE TABLE t1 (a INT);
## Inserting some records in t1 ##
INSERT INTO t1 VALUES (1),(2),(3);
SELECT f1(a) FROM t1;
f1(a)
1
1
1
## Dropping function f1 & table t1 ##
DROP FUNCTION f1;
DROP TABLE t1;
'#--------------------FN_DYNVARS_063_02-------------------------#'
## Switching to default connection ##
## Setting value of variable to 1 ##
SET @@global.log_bin_trust_function_creators = 1;
## Creating and connecting to new connection test_con2 ##
## Verifying value of variable ##
SELECT @@log_bin_trust_function_creators;
@@log_bin_trust_function_creators
1
SELECT @@sql_log_bin;
@@sql_log_bin
1
## Creating new function f1 ##
CREATE FUNCTION f1(a INT) RETURNS INT
BEGIN
IF (a < 3) THEN
INSERT INTO t2 VALUES (a);
END IF;
RETURN 1;
END|
## Creating new table t1 ##
CREATE TABLE t1 (a INT);
## Inserting values in table t1 ##
INSERT INTO t1 VALUES (1),(2),(3);
SELECT f1(a) FROM t1;
f1(a)
1
1
1
## Dropping function f1 ##
DROP FUNCTION f1;
## Dropping table t1 & t2 ##
DROP TABLE t1,t2;
## Disconnecting test_con2##
DROP USER tt@localhost;
SET @@global.log_bin_trust_function_creators= @start_value;

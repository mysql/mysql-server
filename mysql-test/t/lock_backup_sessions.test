--disable_ps_protocol
--enable_connect_log

SET lock_wait_timeout= 1;
SET autocommit= 0;

CREATE USER 'user1'@'localhost';
CREATE USER 'user2'@'localhost';
CREATE USER 'user3'@'localhost';

# User get BACKUP_ADMIN privilege.
GRANT all ON *.* TO 'user1'@'localhost';
GRANT all ON *.* TO 'user2'@'localhost';
GRANT all ON *.* TO 'user3'@'localhost';

# user1 locks ddl of user2 and user3, not his own.
--connect(con1, localhost, user1,)
SET lock_wait_timeout= 1;
SET autocommit= 0;
LOCK INSTANCE FOR BACKUP;
CREATE TABLE testtable_11 (c1 int, c2 varchar(10));
CREATE TEMPORARY TABLE temptable_11 (tt1 int);

--connect(con2, localhost, user2,)
SET lock_wait_timeout= 1;
SET autocommit= 0;
--error ER_LOCK_WAIT_TIMEOUT
CREATE TABLE testtable_21 (c1 int, c2 varchar(10));
CREATE TEMPORARY TABLE temptable_21 (tt1 int);

--connect(con3, localhost, user3,)
SET lock_wait_timeout= 1;
SET autocommit= 0;
--error ER_LOCK_WAIT_TIMEOUT
CREATE TABLE testtable_31 (c1 int, c2 varchar(10));
CREATE TEMPORARY TABLE temptable_31 (tt1 int);

# user1 unlock ddl for user2 and user3.
--connection con1
UNLOCK INSTANCE;

--connection con1
CREATE TABLE testtable_12 (c1 int, c2 varchar(10));

--connection con2
CREATE TABLE testtable_21 (c1 int, c2 varchar(10));

--connection con3
CREATE TABLE testtable_31 (c1 int, c2 varchar(10));

# user2 locks ddl for user1 and user3.
--connection con2
LOCK INSTANCE FOR BACKUP;

--connection con1
--error ER_LOCK_WAIT_TIMEOUT
CREATE TABLE testtable_12 (c1 int, c2 varchar(10));

--connection con3
--error ER_LOCK_WAIT_TIMEOUT
CREATE TABLE testtable_31 (c1 int, c2 varchar(10));

# user1 locks ddl for user2 and user3.
--connection con1
LOCK INSTANCE FOR BACKUP;

# locks are also effective for root.
--connection default
--error ER_LOCK_WAIT_TIMEOUT
CREATE TABLE testtable_1 (c1 int, c2 varchar(10));

--connection con1
--error ER_LOCK_WAIT_TIMEOUT
DROP TABLE IF EXISTS testtable_11;
--error ER_LOCK_WAIT_TIMEOUT
DROP TABLE IF EXISTS testtable_12;

--connection con2
--error ER_LOCK_WAIT_TIMEOUT
DROP TABLE IF EXISTS testtable_21;

--connection con3
--error ER_LOCK_WAIT_TIMEOUT
DROP TABLE IF EXISTS testtable_31;

# unlock of ddl of user2 and user3.
--connection con1
UNLOCK INSTANCE;

# Still locked by user2.
--connection con1
--error ER_LOCK_WAIT_TIMEOUT
DROP TABLE IF EXISTS testtable_11;
--error ER_LOCK_WAIT_TIMEOUT
DROP TABLE IF EXISTS testtable_12;

# unlock ddl of user1 and user3.
--connection con2
UNLOCK INSTANCE;

# Now all dll is unlocked.
--connection con1
DROP TABLE IF EXISTS testtable_11;
DROP TABLE IF EXISTS testtable_12;

--connection con2
DROP TABLE IF EXISTS testtable_21;

--connection con3
DROP TABLE IF EXISTS testtable_31;

--connection default
--disconnect con1
--disconnect con2
--disconnect con3

DROP USER 'user1'@'localhost';
DROP USER 'user2'@'localhost';
DROP USER 'user3'@'localhost';


--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # BUG#30869674 - OPTIMIZE TABLE ON MYISAM CAN INCREASE TABLE SIZE (~2X) AND REDUCE PERFORMANCE
--echo #

--disable_warnings
DROP DATABASE IF EXISTS test_bug30869674;
--enable_warnings

SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE DATABASE test_bug30869674;
USE test_bug30869674;
CREATE TABLE t1(id int, name varchar(255), description varchar(255), count int, primary key(id)) ENGINE=myisam;
INSERT INTO t1 VALUES (1, "test1", "description1", 1), (2, "test2", "description2", 2), (3, "test3", "description3", 3);
FLUSH TABLES;

SELECT * FROM test_bug30869674.t1;

UPDATE t1 SET name="testing test2" WHERE id=2;
FLUSH TABLES;

let $MYSQLD_DATADIR= `select @@datadir`;
--replace_result $MYSQLD_DATADIR MYSQLD_DATADIR
--exec $MYISAMCHK -ei $MYSQLD_DATADIR/test_bug30869674/t1

# echo Optimize table should return the correct number of Records and Linkdata
OPTIMIZE TABLE t1;
--replace_result $MYSQLD_DATADIR MYSQLD_DATADIR
--exec $MYISAMCHK -ei $MYSQLD_DATADIR/test_bug30869674/t1

# Check that the contents are correct.
SELECT * FROM test_bug30869674.t1;

#Cleanup
DROP DATABASE test_bug30869674;

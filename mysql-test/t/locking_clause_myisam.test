--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # Bug#24741298: "SELECT ... FOR UPDATE NOWAIT" ON MYISAM TABLE CAUSED
--echo # CRASH
--echo #

CREATE TABLE t1 ( a INT ) ENGINE = MyISAM;
SELECT * FROM t1 FOR UPDATE NOWAIT;
DROP TABLE t1;

--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo # Test 6: Unsupported storage engines.
--echo #

CREATE TABLE t1(a INT) engine= MyISAM;
--error ER_ILLEGAL_HA
FLUSH TABLE t1 FOR EXPORT;
DROP TABLE t1;

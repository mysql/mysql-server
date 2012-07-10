--echo ####################################
--echo # CLEANUP
--echo ####################################
--disable_warnings
DROP TABLE IF EXISTS t1;
DROP TABLE IF EXISTS t2;
DROP TABLE IF EXISTS t3;
DROP TABLE IF EXISTS t4;
DROP TABLE IF EXISTS t5;
DROP TABLE IF EXISTS t6;
DROP TABLE IF EXISTS t11;
DROP TABLE IF EXISTS t12;
DROP DATABASE IF EXISTS statements_digest;
--enable_warnings

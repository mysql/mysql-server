--echo ####################################
--echo # CLEANUP
--echo ####################################
--disable_warnings
DROP TABLE IF EXISTS t1;
DROP TABLE IF EXISTS t2;
DROP TABLE IF EXISTS t3;
DROP TABLE IF EXISTS t4;
DROP TABLE IF EXISTS t5;
DROP DATABASE IF EXISTS statements_digest;
--enable_warnings

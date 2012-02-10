--echo ####################################
--echo # CLEANUP
--echo ####################################
--disable_warnings
drop table if exists t1;
drop table if exists t2;
drop table if exists t3;
drop table if exists t4;
drop table if exists t5;
drop database if exists statements_digest;
--enable_warnings

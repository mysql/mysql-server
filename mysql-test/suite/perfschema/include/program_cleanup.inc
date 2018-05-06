#
# clean up of set-up created in 
# suite/perfschema/include/program_setup.inc
#

--disable_warnings
DROP PROCEDURE SampleProc1;
DROP PROCEDURE SampleProc2;
DROP PROCEDURE SampleProc3;
DROP PROCEDURE SampleProc4;

DROP FUNCTION wt_avg;
DROP FUNCTION fac;
DROP FUNCTION append;

DROP TRIGGER trg1;
DROP TRIGGER trg2;
DROP TRIGGER trg3;
DROP TRIGGER trg4;
DROP TRIGGER trg5;

DROP EVENT IF EXISTS e1;

DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
DROP TABLE table_t;

DROP DATABASE stored_programs;
--enable_warnings
SET GLOBAL event_scheduler=ON;
--source include/running_event_scheduler.inc


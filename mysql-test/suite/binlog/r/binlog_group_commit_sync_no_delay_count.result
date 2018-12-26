CREATE TABLE t1 (c1 INT) Engine=InnoDB;
CREATE TABLE t2 (c1 INT) Engine=InnoDB;
SET GLOBAL binlog_group_commit_sync_delay=1000000;
SET GLOBAL binlog_group_commit_sync_no_delay_count=2;
######################## ASSERTION #1 #####################################
SET DEBUG_SYNC='bgc_after_flush_stage_before_sync_stage SIGNAL go_con2_to_flush WAIT_FOR go_con1_to_sync';
============== CON1: FLUSHed and REAPed the QUEUE Waiting for CON2 to get to the FLUSH stage ============
SET @clock_in = SYSDATE();
INSERT INTO t1 VALUES (1);
SET DEBUG_SYNC='bgc_before_flush_stage SIGNAL go_con1_to_sync WAIT_FOR go_con2_to_flush';
SET DEBUG_SYNC='bgc_after_flush_stage_before_sync_stage WAIT_FOR go_con2_to_sync';
============== CON2: WILL FLUSH and REAP the QUEUE but only after CON1 has waited and signaled CON2 ============
SET @clock_in = SYSDATE();
INSERT INTO t2 VALUES (2);
SET @elapsed = TIMESTAMPDIFF(MICROSECOND, @clock_in, SYSDATE());
============== CON1: signal CON2 to resume and go into SYNC stage ============
SET DEBUG_SYNC = "now SIGNAL go_con2_to_sync";
include/assert.inc ["Assert that con1 statement took more than the delay set."]
============== CON2: RESUMES ============
SET @elapsed = TIMESTAMPDIFF(MICROSECOND, @clock_in, SYSDATE());
include/assert.inc ["Assert that con2 statement took more than the delay set."]
######################## ASSERTION #2 #####################################
SET GLOBAL DEBUG="d,bgc_set_infinite_delay";
============== CON1: enters wait_count_or_timeout waits for CON2 to enroll for the SYNC stage ============
SET DEBUG_SYNC='bgc_after_flush_stage_before_sync_stage WAIT_FOR go_con1_to_wait';
SET DEBUG_SYNC='bgc_wait_count_or_timeout SIGNAL go_con2_to_flush WAIT_FOR go_con1_resume_wait';
SET @clock_in = SYSDATE();
INSERT INTO t1 VALUES (1);
============== CON2: signals CON1 that it has enrolled for the SYNC stage ============
SET DEBUG_SYNC='bgc_before_flush_stage SIGNAL go_con1_to_wait WAIT_FOR go_con2_to_flush';
SET DEBUG_SYNC='bgc_after_enrolling_for_sync_stage SIGNAL go_con1_resume_wait';
INSERT INTO t2 VALUES (2);
SET GLOBAL DEBUG="";;
SET GLOBAL binlog_group_commit_sync_no_delay_count=0;
SET GLOBAL binlog_group_commit_sync_delay=0;
DROP TABLE t1, t2;

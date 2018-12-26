--source include/have_log_bin.inc

--echo #
--echo # Bug#21041908 -  events+rpl: void close_thread_tables(thd*):
--echo #                 assertion `thd->get_transaction()
--echo #

SET GLOBAL EVENT_SCHEDULER = OFF;
--source include/no_running_event_scheduler.inc

CREATE EVENT e1 ON SCHEDULE EVERY 1 SECOND
 ENDS NOW() + INTERVAL 1 SECOND DO SELECT 1;

--sleep 1
SET GLOBAL EVENT_SCHEDULER = ON;
--source include/running_event_scheduler.inc

--disable_warnings
DROP EVENT IF EXISTS e1;
--enable_warnings

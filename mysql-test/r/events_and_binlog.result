#
# Bug#21041908 -  events+rpl: void close_thread_tables(thd*):
#                 assertion `thd->get_transaction()
#
SET GLOBAL EVENT_SCHEDULER = OFF;
CREATE EVENT e1 ON SCHEDULE EVERY 1 SECOND
ENDS NOW() + INTERVAL 1 SECOND DO SELECT 1;
SET GLOBAL EVENT_SCHEDULER = ON;
DROP EVENT IF EXISTS e1;

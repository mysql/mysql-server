SET DEBUG_SYNC = "now SIGNAL log_recent_closed_unfreeze";
--connection con_log_recent_closed_freeze
reap;
COMMIT;
DROP TABLE log_recent_closed_freeze_t;
disconnect con_log_recent_closed_freeze;
--connection default

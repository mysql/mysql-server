#
# Bug 83003: Using temporary tables on slaves increases GTID sequence number
#
RESET MASTER;
CREATE TEMPORARY TABLE temp_non_trx(a INT) ENGINE=MyISAM;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info

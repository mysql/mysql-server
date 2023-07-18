# When log-bin, skip-log-bin and binlog-format options are specified, mask the warning.

--disable_query_log
call mtr.add_suppression("\\[Warning\\] \\[[^]]*\\] \\[[^]]*\\] You need to use --log-bin to make --binlog-format work.");
call mtr.add_suppression("\\[Warning\\] \\[[^]]*\\] \\[[^]]*\\] You need to use --log-bin to make --binlog-expire-logs-seconds work.");
--enable_query_log

#
# Bug#17733 Flushing logs causes daily server crash
#

select @@GLOBAL.binlog_expire_logs_seconds into @save_binlog_expire_logs_seconds;
# The hostname needs to be stripped off its extensions as even
# in code we use the stripped hostname as default basename
--let $HOST_NAME= `SELECT SUBSTRING_INDEX(@@hostname, '.', 1)`
flush logs;
set global binlog_expire_logs_seconds = 259200;
show variables like 'log_bin%';
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR $HOST_NAME hostname
show variables like 'relay_log%';
flush logs;
show variables like 'log_bin%';
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR $HOST_NAME hostname
show variables like 'relay_log%';
set @@GLOBAL.binlog_expire_logs_seconds = @save_binlog_expire_logs_seconds;

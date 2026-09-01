--echo
--echo ## Test case initialization - Begin

--source include/no_protocol.inc

--echo
--echo # Storage quantities allow up to ULONG_MAX - 1
set @ulong_max = 18446744073709551615;
set @max_level = @ulong_max - 1;

--echo
--echo # A very high offline level will force the offline mode
set @force_offline_mode = @max_level;

--echo
--echo # Disk usage warning level = 100% will force warnings, 0% will disable warnings
set @force_disk_warning = 100;
set @no_disk_warning = 0;

--echo
--echo # Memory usage warning level = ULONG_MAX-1 will force warnings, 0 will disable warnings
set @force_memory_warning = @max_level;
set @no_memory_warning = 0;

--echo
--echo # Ignore warnings during startup
# Disable binary logging for replication tests
set session sql_log_bin = 0;
call mtr.add_suppression("Disk: Normal");
call mtr.add_suppression("Disk: Warning");
call mtr.add_suppression("Disk: Low Level");
call mtr.add_suppression("Disk: Critical");
call mtr.add_suppression("Disk: Error");
call mtr.add_suppression("Disk: Available space");
call mtr.add_suppression("Disk: Fallback");
call mtr.add_suppression("Disk: Recovery");
call mtr.add_suppression("Disk: Maximum recovery cycles");
call mtr.add_suppression("Disk: OFFLINE_MODE");
call mtr.add_suppression("Disk: SUPER_READ_ONLY");
call mtr.add_suppression("Disk Collector: Error");
call mtr.add_suppression("Memory: Warning");
# Enable binary logging
set session sql_log_bin = 1;

--echo
--echo # Install the component
--eval install component 'file://component_health_monitor'

--echo
--echo # Turn off the collectors
set @@global.health_monitor.periodic_reporting = off;
set @@global.health_monitor.memory_reporting = off;
set @@global.health_monitor.disk_running = off;
set @@global.health_monitor.memory_running = off;
set @@global.health_monitor.system_running = off;

--echo
--echo # Disable low disk space warnings
set @@global.health_monitor.disk_usage_warning_level_1 = @no_disk_warning;
set @@global.health_monitor.disk_usage_warning_level_2 = @no_disk_warning;
set @@global.health_monitor.disk_usage_warning_level_3 = @no_disk_warning;

--echo
--echo # Disable low memory warnings
set @@global.health_monitor.memory_usage_warning_level_1 = @no_memory_warning;
set @@global.health_monitor.memory_usage_warning_level_2 = @no_memory_warning;
set @@global.health_monitor.memory_usage_warning_level_3 = @no_memory_warning;

--echo
--echo # Disable the low level storage thresholds to avoid inadvertently triggering
--echo # fallback mode on the vardir volume
set @@global.health_monitor.disk_low_space_level = 0;
set @@global.health_monitor.disk_low_space_percent = 0;
set @@global.health_monitor.disk_low_space_critical_level = 0;

--echo # Disable recovery mode
set @@global.health_monitor.disk_recovery_enable = off;

--echo
--echo # Fallback mode may have been triggered when the component was loaded,
--echo # so reset offline_mode and super_read_only as a precaution.
--echo # The 'super_read_only_disk_full' status variable will be reset when the
--echo # monitored disk path is changed.
--echo
set @@global.super_read_only = off;
set @@global.read_only = off;
set @@global.offline_mode = off;

--echo
--echo # Set monitored disk after disabling the low space thresholds
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval set @@global.health_monitor.disk_monitored = '$MYSQLTEST_VARDIR'

--echo
select @@global.offline_mode;
select @@global.super_read_only;
select @@global.read_only;
show status like 'super_read_only_disk_full';
show status like 'offline_mode_disk_full';

--echo
use performance_schema;

--echo
--echo ## Test case initialization - End

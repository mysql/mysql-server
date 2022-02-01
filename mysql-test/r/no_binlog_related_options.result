call mtr.add_suppression("You need to use --log-bin to make --binlog-format work.");
call mtr.add_suppression("You need to use --log-bin to make --log-slave-updates work.");
SELECT @@GLOBAL.log_bin;
@@GLOBAL.log_bin
0
#
# Verify that the log-slave-updates warning is printed when starting
# the server and log-slave-updates is enabled as before, if both
# --skip-log-bin and --log-slave-updates options are set explicitly
# on command line or configuration file.
#
include/assert_grep.inc [Found the expected warning "You need to use --log-bin to make --log-slave-updates work." in server's error log.]
SELECT @@GLOBAL.log_slave_updates;
@@GLOBAL.log_slave_updates
1

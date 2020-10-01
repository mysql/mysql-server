#
# Testing wl#10441: Add mysqld_safe-functionality to server
#
# Shutdown mysqld which is started by mtr.
# Run -I on a new datadir
#
# Deduce --basedir when using full path to mysqld
# Look for [ERROR] in error log (there should be none):
#
# Deduce --basedir when using path relative to CWD
# Look for [ERROR] in error log (there should be none):
#
# Deduce --basedir when using bare executable name (PATH lookup)
# Look for [ERROR] in error log (there should be none):
#
# Try invalid --log-error
# Look for expected error in output:
# Found expected error
#
# Try -D as shortcut for --daemonize option with invalid --log-error
# Look for expected errors in output from launcher and daemon:
# Found expected error from launcher process
# Found expected error from daemon process
#
# Try using -D with relative path
# Look for [ERROR] in error log (there should be none):
# Look for output (there should be none):
# Supressing output for initialize:
# Wait for daemon server to start
# Execute a query to see that it is running OK
SHOW DATABASES;
Database
information_schema
mtr
mysql
performance_schema
sys
test
# Shutdown daemon
# Wait until daemon is gone
#
# Cleanup
#
#
# Restart mysqld of mtr
#

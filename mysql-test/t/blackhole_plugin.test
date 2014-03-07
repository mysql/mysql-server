--source include/not_windows.inc
--source include/have_blackhole_plugin.inc

# When running in parallel we get
# Warning 1620 Plugin is busy and will be uninstalled on shutdown
--source include/not_parallel.inc

--ERROR ER_UNKNOWN_STORAGE_ENGINE
CREATE TABLE t1(a int) ENGINE=BLACKHOLE;
DROP TABLE IF EXISTS t1;

eval INSTALL PLUGIN blackhole SONAME 'ha_blackhole.so';
--error 1125
eval INSTALL PLUGIN BLACKHOLE SONAME 'ha_blackhole.so';

UNINSTALL PLUGIN blackhole;

eval INSTALL PLUGIN blackhole SONAME 'ha_blackhole.so';

CREATE TABLE t1(a int) ENGINE=BLACKHOLE;

DROP TABLE t1;

# This dummy statement is required for --ps-protocol mode.
# The thing is that last prepared statement is "cached" in mysqltest.cc
# (unless "reconnect" is enabled, and that's not the case here).
# This statement forces mysqltest.cc to close prepared "DROP TABLE t1".
# Otherwise, the plugin can not be uninstalled because there is an active
# prepared statement using it.
SELECT 1;

UNINSTALL PLUGIN blackhole;
--error ER_SP_DOES_NOT_EXIST
UNINSTALL PLUGIN blackhole;


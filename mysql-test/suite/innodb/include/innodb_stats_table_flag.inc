#
-- echo =====
-- echo === Test ANALYZE behavior after default creation
-- echo =====

CREATE TABLE test_ps_flag (a INT) ENGINE=INNODB;

-- source suite/innodb/include/innodb_stats_table_flag_analyze.inc

DROP TABLE test_ps_flag;

CREATE TABLE test_ps_flag (a INT) ENGINE=INNODB STATS_PERSISTENT=default;

-- source suite/innodb/include/innodb_stats_table_flag_analyze.inc

DROP TABLE test_ps_flag;

#
-- echo =====
-- echo === Test ANALYZE behavior after creation with explicit PS=OFF
-- echo =====

CREATE TABLE test_ps_flag (a INT) ENGINE=INNODB STATS_PERSISTENT=0;

-- source suite/innodb/include/innodb_stats_table_flag_analyze.inc

DROP TABLE test_ps_flag;

#
-- echo =====
-- echo === Test ANALYZE behavior after creation with explicit PS=ON
-- echo =====

CREATE TABLE test_ps_flag (a INT) ENGINE=INNODB STATS_PERSISTENT=1;

-- source suite/innodb/include/innodb_stats_table_flag_analyze.inc

DROP TABLE test_ps_flag;

#
-- echo =====
-- echo === Test ANALYZE behavior after creation with explicit PS=OFF,
-- echo === then ALTER to ON, then ALTER to OFF, then ALTER to default
-- echo =====

CREATE TABLE test_ps_flag (a INT) ENGINE=INNODB STATS_PERSISTENT=0;

ALTER TABLE test_ps_flag STATS_PERSISTENT=1;

# also check that the change from the ALTER TABLE survives server restart
-- source include/restart_mysqld.inc

-- source suite/innodb/include/innodb_stats_table_flag_analyze.inc

ALTER TABLE test_ps_flag STATS_PERSISTENT=0;

-- source suite/innodb/include/innodb_stats_table_flag_analyze.inc

ALTER TABLE test_ps_flag STATS_PERSISTENT=default;

-- source suite/innodb/include/innodb_stats_table_flag_analyze.inc

DROP TABLE test_ps_flag;

#
-- echo =====
-- echo === Test ANALYZE behavior after creation with explicit PS=ON,
-- echo === then ALTER to OFF, then ALTER to ON, then ALTER to default
-- echo =====

CREATE TABLE test_ps_flag (a INT) ENGINE=INNODB STATS_PERSISTENT=1;

ALTER TABLE test_ps_flag STATS_PERSISTENT=0;

# also check that the change from the ALTER TABLE survives server restart
-- source include/restart_mysqld.inc

-- source suite/innodb/include/innodb_stats_table_flag_analyze.inc

ALTER TABLE test_ps_flag STATS_PERSISTENT=1;

-- source suite/innodb/include/innodb_stats_table_flag_analyze.inc

ALTER TABLE test_ps_flag STATS_PERSISTENT=default;

-- source suite/innodb/include/innodb_stats_table_flag_analyze.inc

DROP TABLE test_ps_flag;

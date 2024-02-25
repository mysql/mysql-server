#
# Tests for INFORMATION_SCHEMA system views requiring debug build of server.
#
--source include/have_debug.inc
--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # Bug#24487414 - SIG 11 IN DD::INFO_SCHEMA::STATISTICS_CACHE::READ_STAT_BY_OPEN_TABLE
--echo #
SET SESSION information_schema_stats_expiry=0;

# time_zone table name is used here to set debug point
# kill_query_on_open_table_from_tz_find from the
# simulate_kill_query_on_open_table debug point.
CREATE TABLE time_zone(f1 INT PRIMARY KEY) ENGINE=MyISAM;
INSERT INTO time_zone VALUES (10);

SET SESSION DEBUG="+d,simulate_kill_query_on_open_table";
--echo # Without fix, following query results in crash when query is killed while
--echo # opening "test.time_zone" table.
--error ER_QUERY_INTERRUPTED
SELECT * FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA='test' AND
                                                  TABLE_NAME = 'time_zone';
SET SESSION DEBUG="-d,simulate_kill_query_on_open_table";

DROP TABLE time_zone;
SET SESSION information_schema_stats_expiry=default;


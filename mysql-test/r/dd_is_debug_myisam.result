#
# Bug#24487414 - SIG 11 IN DD::INFO_SCHEMA::STATISTICS_CACHE::READ_STAT_BY_OPEN_TABLE
#
SET SESSION information_schema_stats_expiry=0;
CREATE TABLE time_zone(f1 INT PRIMARY KEY) ENGINE=MyISAM;
INSERT INTO time_zone VALUES (10);
SET SESSION DEBUG="+d,simulate_kill_query_on_open_table";
# Without fix, following query results in crash when query is killed while
# opening "test.time_zone" table.
SELECT * FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA='test' AND
TABLE_NAME = 'time_zone';
ERROR 70100: Query execution was interrupted
SET SESSION DEBUG="-d,simulate_kill_query_on_open_table";
DROP TABLE time_zone;
SET SESSION information_schema_stats_expiry=default;

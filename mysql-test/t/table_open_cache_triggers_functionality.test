--source include/no_ps_protocol.inc

--echo #
--echo # Test coverage for limit on number of table objects in Table Cache
--echo # with fully-loaded triggers.
--echo #

--echo
--echo # Create some test tables with and without triggers.
CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TRIGGER bi_t1 BEFORE INSERT ON t1 FOR EACH ROW SET @a = @a + 1;
CREATE TABLE t2 (j INT);
INSERT INTO t2 VALUES (1);
CREATE TRIGGER ai_t2 AFTER INSERT ON t2 FOR EACH ROW SET @b = @b + 1;
CREATE TABLE t3 (k INT);
INSERT INTO t3 VALUES (1);
CREATE TRIGGER bi_t3 BEFORE INSERT ON t3 FOR EACH ROW SET @c = @c + 1;
CREATE TABLE t4 (l INT);
INSERT INTO t4 VALUES (1);

--echo
--echo # Set small soft limit on number of table objects in Table Cache with
--echo # fully-loaded triggers, so we can observe it kicking in.
SET @save_table_open_cache_triggers = @@global.table_open_cache_triggers;
SET GLOBAL table_open_cache_triggers = 2;

--echo # To make further test repeatable reset Table Cache.
FLUSH TABLES;
--echo # But Ensure that it contains necessary DD tables.
--disable_result_log
--disable_query_log
CREATE TABLE t0 (h INT);
INSERT INTO t0 VALUES (1);
CREATE TRIGGER bi_t0 BEFORE UPDATE ON t0 FOR EACH ROW SET @h = @h + 1;
SELECT COUNT(*) FROM t0;
DROP TABLE t0;
SELECT variable_value FROM performance_schema.session_status;
--enable_query_log
--enable_result_log
--echo # Reset status variables to get 0 as their base value.
FLUSH STATUS;

--echo
--echo # Read-only statements should not need table objects with fully-loaded
--echo # triggers and thus change status variables tracking their usage.
--echo # Otherwise absence of object in table cache causes TC miss.
--echo # presence - TC hit. Read-only statement requiring 2 table objects
--echo # when only 1 is present in TC causes 1 hit and 1 miss.
SELECT count(*) FROM t1;

--let $expected_inc = { "tc_misses_inc": 1, "trg_hits_inc": 0, "trg_misses_inc" : 0, "trg_overflows_inc" : 0 }
--source include/assert_trigger_cache_increment.inc

SELECT count(*) FROM t1 AS a, t1 AS b;

--let $expected_inc = { "tc_misses_inc": 1, "trg_hits_inc": 0, "trg_misses_inc" : 0, "trg_overflows_inc" : 0 }
--source include/assert_trigger_cache_increment.inc

SELECT count(*) FROM t4;

--let $expected_inc = { "tc_misses_inc": 1, "trg_hits_inc": 0, "trg_misses_inc" : 0, "trg_overflows_inc" : 0 }
--source include/assert_trigger_cache_increment.inc

--echo
--echo # Updating statement needs fully-loaded triggers, so there will be
--echo # a table cache hit, but also trigger miss and trigger will be
--echo # loaded.
INSERT INTO t1 VALUES (4);

--let $expected_inc = { "tc_misses_inc": 0, "trg_hits_inc": 0, "trg_misses_inc" : 1, "trg_overflows_inc" : 0 }
--source include/assert_trigger_cache_increment.inc

--echo
--echo # Repeating the statement will cause table cache and trigger hits.
INSERT INTO t1 VALUES (5);

--let $expected_inc = { "tc_misses_inc": 0, "trg_hits_inc": 1, "trg_misses_inc" : 0, "trg_overflows_inc" : 0 }
--source include/assert_trigger_cache_increment.inc

--echo
--echo # When same statement needs 2 instances of the table, one for
--echo # updating and another for read-only purposes, then for updating
--echo # we should try to return object with loaded triggers, and object
--echo # without triggers we also have for read-only part.
INSERT INTO t1 SELECT * FROM t1;

--let $expected_inc = { "tc_misses_inc": 0, "trg_hits_inc": 1, "trg_misses_inc" : 0, "trg_overflows_inc" : 0 }
--source include/assert_trigger_cache_increment.inc

--echo
--echo # Statement needing 2 table objects with triggers will cause one
--echo # trigger hit and one trigger miss (causing trigger loading for
--echo # one of cached table objects).
LOCK TABLES t1 AS a WRITE, t1 AS b WRITE;

--let $expected_inc = { "tc_misses_inc": 0, "trg_hits_inc": 1, "trg_misses_inc" : 1, "trg_overflows_inc" : 0 }
--source include/assert_trigger_cache_increment.inc

UNLOCK TABLES;

--let $expected_inc = { "tc_misses_inc": 0, "trg_hits_inc": 0, "trg_misses_inc" : 0, "trg_overflows_inc" : 0 }
--source include/assert_trigger_cache_increment.inc

--echo
--echo # Running read-only statement on unrelated table should not disturb
--echo # trigger counters. Since only one table object for this table was
--echo # used before it will cause single TC miss as expected.
SELECT count(*) FROM t4 AS a, t4 AS b;

--let $expected_inc = { "tc_misses_inc": 1, "trg_hits_inc": 0, "trg_misses_inc" : 0, "trg_overflows_inc" : 0 }
--source include/assert_trigger_cache_increment.inc

--echo
--echo # Statement which needs 3 table objects for updating will cause 2
--echo # trigger hits and 1 trigger (and TC) miss . As result we temporarily
--echo # exceed the soft limit on open tables with triggers.
LOCK TABLES t1 AS a WRITE, t1 AS b WRITE, t1 AS c WRITE;

--let $expected_inc = { "tc_misses_inc": 1, "trg_hits_inc": 2, "trg_misses_inc" : 1, "trg_overflows_inc" : 0 }
--source include/assert_trigger_cache_increment.inc

--echo
--echo # Once we are done with using all 3 table objects, one table
--echo # object with trigger gets evicted. 2 table objects with triggers
--echo # is left.
UNLOCK TABLES;

--let $expected_inc = { "tc_misses_inc": 0, "trg_hits_inc": 0, "trg_misses_inc" : 0, "trg_overflows_inc" : 1 }
--source include/assert_trigger_cache_increment.inc

--echo
--echo # Read-only statement will use table objects with triggers and
--echo # should not disturb trigger counters. Since we have not used
--echo # table t2 so far, there will be 1 TC miss.
SELECT count(*) FROM t1, t2;

--let $expected_inc = { "tc_misses_inc": 1, "trg_hits_inc": 0, "trg_misses_inc" : 0, "trg_overflows_inc" : 0 }
--source include/assert_trigger_cache_increment.inc

--echo
--echo # Statement that uses t1 for updating and table t2 in read-only
--echo # mode will reuse 1 table object with triggers (and also reuse object
--echo # without triggers for t2), and won't disturb cache otherwise.
INSERT INTO t1 SELECT * FROM t2;

--let $expected_inc = { "tc_misses_inc": 0, "trg_hits_inc": 1, "trg_misses_inc" : 0, "trg_overflows_inc" : 0 }
--source include/assert_trigger_cache_increment.inc

--echo
--echo # Statement that updates t2 will require loading of triggers (existing
--echo # table object will be used for this), this will cause table object
--echo # with triggers for table t1 eviction.
INSERT INTO t2 VALUES (2);

--let $expected_inc = { "tc_misses_inc": 0, "trg_hits_inc": 0, "trg_misses_inc" : 1, "trg_overflows_inc" : 1 }
--source include/assert_trigger_cache_increment.inc

--echo
--echo # Statement that updates t3 will require new table object and triggers
--echo # loaded for it. Since Table Cache already has 2 table objects with
--echo # triggers it will cause eviction of one of these objects. According
--echo # to LRU it should be object for t1.
INSERT INTO t3 VALUES (2);

--let $expected_inc = { "tc_misses_inc": 1, "trg_hits_inc": 0, "trg_misses_inc" : 1, "trg_overflows_inc" : 1 }
--source include/assert_trigger_cache_increment.inc

--echo
--echo # Let us confirm that it is table object for t1 which is gone
--echo # missing, by showing that it and its triggers need to be loaded again.
INSERT INTO t1 VALUES (6);

--let $expected_inc = { "tc_misses_inc": 1, "trg_hits_inc": 0, "trg_misses_inc" : 1, "trg_overflows_inc" : 1 }
--source include/assert_trigger_cache_increment.inc

--echo #
--echo # Clean up.
--echo #
DROP TABLES t1, t2, t3, t4;
SET GLOBAL table_open_cache_triggers = @save_table_open_cache_triggers;
# Clean up JSON helper scripts created by assert_trigger_cache_increment.inc
--source include/destroy_json_functions.inc

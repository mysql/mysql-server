#
# Bug#32508608: MDLS FOR ALTER TABLE SECONDARY_LOAD ARE TOO STRICT
#
CREATE TABLE t1 (a INT) SECONDARY_ENGINE MOCK;
INSERT INTO t1 VALUES (1),(2),(3);
SELECT * FROM t1;
a
1
2
3
# Starting reader connection
# Run query which will block while holding shared MDL
SET DEBUG_SYNC="after_open_table_mdl_shared SIGNAL holding_shared_mdl WAIT_FOR finish_select";
SELECT * FROM t1;
# Switch to default connection
# Wait until reader has acquired shared MDL
SET DEBUG_SYNC="now WAIT_FOR holding_shared_mdl";
# Verify that load can proceed to the point where it needs to
# upgrade to X-MDL, and then signal the reader con to proceed
SET DEBUG_SYNC="secload_upgrade_mdl_x SIGNAL finish_select";
ALTER TABLE t1 SECONDARY_LOAD;
SET DEBUG_SYNC="RESET";
# Switch to reader con and wait for query to complete
a
# Switch to default connection
# Test error handling in secondary unload
SET DEBUG="+d,sim_secunload_fail";
ALTER TABLE t1 SECONDARY_UNLOAD;
ERROR HY000: Secondary engine operation failed. Simulated failure of secondary_unload().
SET DEBUG="-d,sim_secunload_fail";
SELECT * FROM t1;
a
ALTER TABLE t1 SECONDARY_UNLOAD;
# Test error handling in secondary unload
SET DEBUG="+d,sim_secload_fail";
ALTER TABLE t1 SECONDARY_LOAD;
ERROR HY000: Secondary engine operation failed. Simulated failure of secondary_load().
SET DEBUG="-d,sim_secload_fail";
SELECT * FROM t1;
a
1
2
3
# Cleanup
DROP TABLE t1;

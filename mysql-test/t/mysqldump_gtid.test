--echo #
--echo # WL#12959: Contribution by Facebook: Added optional commenting of the
--echo #   @@GLOBAL.GTID_PURGED by dump
--echo #

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
INSERT INTO t1 VALUES(3);

-- let $MASTER_UUID = `SELECT @@SERVER_UUID;`

--echo # Success criteria: SET GTID_PURGED should be commented
-- replace_result $MASTER_UUID uuid
-- exec $MYSQL_DUMP --skip_comments --single_transaction --set_gtid_purged=COMMENTED --dump_date=false test

DROP TABLE t1;

RESET BINARY LOGS AND GTIDS;

--echo # End of 8.0 tests

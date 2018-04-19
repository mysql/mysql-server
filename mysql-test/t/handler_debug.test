--source include/have_debug.inc

--echo #
--echo # Bug#21906010 - ASSERT DIAGNOSTICS_AREA::SET_EOF_STATUS(THD*):
--echo #                ASSERTION !IS_SET() FOR HANDLERS
--echo #
CREATE TABLE t(c1 INT, KEY k1(c1));
INSERT INTO t VALUES (1),(2),(3),(5),(7);
HANDLER t OPEN;
SET DEBUG='+d,simulate_handler_read_failure';
--error ER_NET_ERROR_ON_WRITE
HANDLER t READ k1= (5);
SET DEBUG='-d,simulate_handler_read_failure';
HANDLER t CLOSE;
DROP TABLE t;

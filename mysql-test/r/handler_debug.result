#
# Bug#21906010 - ASSERT DIAGNOSTICS_AREA::SET_EOF_STATUS(THD*):
#                ASSERTION !IS_SET() FOR HANDLERS
#
CREATE TABLE t(c1 INT, KEY k1(c1));
INSERT INTO t VALUES (1),(2),(3),(5),(7);
HANDLER t OPEN;
SET DEBUG='+d,simulate_handler_read_failure';
HANDLER t READ k1= (5);
ERROR 08S01: Got an error writing communication packets
SET DEBUG='-d,simulate_handler_read_failure';
HANDLER t CLOSE;
DROP TABLE t;

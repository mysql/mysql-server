include/rpl_init.inc [topology=none]
include/rpl_default_connections.inc
CREATE TABLE t1(i INT);
INSERT INTO t1 values (1);
include/rpl_restart_server.inc [server_number=1 parameters: --gtid-mode=off]
CREATE TABLE t2(i INT);
INSERT INTO t2 values (2);
include/rpl_restart_server.inc [server_number=1 parameters: --gtid-mode=on]
CREATE TABLE t3(i INT);
INSERT INTO t3 values (3);
FLUSH LOGS;
DROP TABLE t1,t2,t3;
RESET MASTER;
SELECT * FROM t1;
i
1
SELECT * FROM t2;
i
2
DROP TABLE t1,t2;

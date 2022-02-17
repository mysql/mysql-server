CREATE USER 'user1'@localhost;
GRANT USAGE ON *.* TO 'user1'@localhost;
SET GLOBAL global_connection_memory_tracking = ON;
# Check OOM for init_connect.
SET GLOBAL init_connect = 'show variables';
# Global limit.
SET GLOBAL global_connection_memory_limit = 1;
# Connection con1
SELECT 1;
ERROR 08S01: Aborted connection to db: 'test' user: 'user1' host: 'localhost' (init_connect command failed)
SELECT PRIO, SUBSYSTEM, DATA FROM performance_schema.error_log
ORDER BY LOGGED DESC LIMIT 1;
PRIO	SUBSYSTEM	DATA
Warning	Server	Aborted connection to db: 'test' user: 'user1' host: 'localhost' (init_connect command failed; diagnostics area: MY-NNNNNN - Connection closed. Global connection memory limit 1 bytes exceed)
SET GLOBAL global_connection_memory_limit = default;
# Connection limit.
SET GLOBAL connection_memory_limit = 1;
# Connection con1
SELECT 1;
ERROR 08S01: Aborted connection to db: 'test' user: 'user1' host: 'localhost' (init_connect command failed)
SELECT PRIO, SUBSYSTEM, DATA FROM performance_schema.error_log
ORDER BY LOGGED DESC LIMIT 1;
PRIO	SUBSYSTEM	DATA
Warning	Server	Aborted connection to db: 'test' user: 'user1' host: 'localhost' (init_connect command failed; diagnostics area: MY-NNNNNN - Connection closed. Connection memory limit 1 bytes exceeded. Con)
SET GLOBAL connection_memory_limit = default;
SET GLOBAL init_connect = default;
# Check OOM before idle stage at connection stage.
# Global limit.
SET GLOBAL global_connection_memory_limit = 1;
# Connection con1
SELECT 1;
ERROR HY000: Connection closed. Global connection memory limit 1 bytes exceeded. Consumed SOME bytes.
SELECT PRIO, SUBSYSTEM, DATA FROM performance_schema.error_log
ORDER BY LOGGED DESC LIMIT 1;
PRIO	SUBSYSTEM	DATA
Error	Server	Connection closed. Global connection memory limit 1 bytes exceeded. Consumed SOME bytes.
SET GLOBAL global_connection_memory_limit = default;
# Connection limit.
SET GLOBAL connection_memory_limit = 1;
# Connection con1
SELECT 1;
ERROR HY000: Connection closed. Connection memory limit 1 bytes exceeded. Consumed SOME bytes.
SELECT PRIO, SUBSYSTEM, DATA FROM performance_schema.error_log
ORDER BY LOGGED DESC LIMIT 1;
PRIO	SUBSYSTEM	DATA
Error	Server	Connection closed. Connection memory limit 1 bytes exceeded. Consumed SOME bytes.
SET GLOBAL connection_memory_limit = default;
# Check OOM before idle stage after succesful statement execution.
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO 'user1'@localhost;
# Global limit.
SET SESSION DEBUG="+d,mem_cnt_no_error_on_exec_global";
SET GLOBAL global_connection_memory_limit = 2;
# Both statements should be executed without errors.
SHOW VARIABLES;
SHOW VARIABLES;
SET GLOBAL global_connection_memory_limit = default;
# Session limit.
SET GLOBAL DEBUG="+d,mem_cnt_no_error_on_exec_session";
SET GLOBAL connection_memory_limit = 10000;
# Both statements should be executed without errors.
SHOW VARIABLES;
SHOW VARIABLES;
SET GLOBAL DEBUG = default;
SET GLOBAL global_connection_memory_limit = default;
SET GLOBAL connection_memory_limit = default;
SET GLOBAL global_connection_memory_tracking = default;
#
#  Bug#33869004 Range optimizer is not aware of the session memory limit.
#
CREATE TABLE t (
i VARBINARY(255), j VARBINARY(255), k VARBINARY(255),
l VARBINARY(255), m VARBINARY(255), n VARBINARY(255),
PRIMARY KEY(i, j, k, l)
);
SET GLOBAL connection_memory_limit = 10485760;
# Connection con1
EXPLAIN SELECT * FROM t FORCE INDEX (PRIMARY) WHERE
i IN ("1", "2", "3", "4", "5", "6", "7", "8", "9", "10") AND
j IN ("1", "2", "3", "4", "5", "6", "7", "8", "9", "10") AND
k IN ("1", "2", "3", "4", "5", "6", "7", "8", "9", "10") AND
l IN ("1", "2", "3", "4", "5", "6", "7", "8", "9", "10");
ERROR HY000: Connection closed. Connection memory limit 10485760 bytes exceeded. Consumed SOME bytes.
SET GLOBAL connection_memory_limit = default;
DROP TABLE t;
DROP USER 'user1'@localhost;

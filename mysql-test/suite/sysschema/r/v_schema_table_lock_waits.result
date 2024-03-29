DESC sys.schema_table_lock_waits;
Field	Type	Null	Key	Default	Extra
object_schema	varchar(64)	YES		NULL	
object_name	varchar(64)	YES		NULL	
waiting_thread_id	bigint unsigned	NO		NULL	
waiting_pid	bigint unsigned	YES		NULL	
waiting_account	text	YES		NULL	
waiting_lock_type	varchar(32)	NO		NULL	
waiting_lock_duration	varchar(32)	NO		NULL	
waiting_query	longtext	YES		NULL	
waiting_query_secs	bigint	YES		NULL	
waiting_query_rows_affected	bigint unsigned	YES		NULL	
waiting_query_rows_examined	bigint unsigned	YES		NULL	
blocking_thread_id	bigint unsigned	NO		NULL	
blocking_pid	bigint unsigned	YES		NULL	
blocking_account	text	YES		NULL	
blocking_lock_type	varchar(32)	NO		NULL	
blocking_lock_duration	varchar(32)	NO		NULL	
sql_kill_blocking_query	varchar(31)	YES		NULL	
sql_kill_blocking_connection	varchar(25)	YES		NULL	
SELECT * FROM sys.schema_table_lock_waits;
DESC sys.x$schema_table_lock_waits;
Field	Type	Null	Key	Default	Extra
object_schema	varchar(64)	YES		NULL	
object_name	varchar(64)	YES		NULL	
waiting_thread_id	bigint unsigned	NO		NULL	
waiting_pid	bigint unsigned	YES		NULL	
waiting_account	text	YES		NULL	
waiting_lock_type	varchar(32)	NO		NULL	
waiting_lock_duration	varchar(32)	NO		NULL	
waiting_query	longtext	YES		NULL	
waiting_query_secs	bigint	YES		NULL	
waiting_query_rows_affected	bigint unsigned	YES		NULL	
waiting_query_rows_examined	bigint unsigned	YES		NULL	
blocking_thread_id	bigint unsigned	NO		NULL	
blocking_pid	bigint unsigned	YES		NULL	
blocking_account	text	YES		NULL	
blocking_lock_type	varchar(32)	NO		NULL	
blocking_lock_duration	varchar(32)	NO		NULL	
sql_kill_blocking_query	varchar(31)	YES		NULL	
sql_kill_blocking_connection	varchar(25)	YES		NULL	
SELECT * FROM sys.x$schema_table_lock_waits;
CREATE TABLE test.lock_waits (i INT PRIMARY KEY, j INT) ENGINE = InnoDB;
LOCK TABLE lock_waits WRITE;
ALTER TABLE test.lock_waits ADD k VARCHAR(20);
SELECT SLEEP(1);
SLEEP(1)
0
SELECT object_schema, object_name,
waiting_account, waiting_lock_type, waiting_query,
blocking_account, blocking_lock_type, blocking_lock_duration
FROM sys.schema_table_lock_waits;
object_schema	object_name	waiting_account	waiting_lock_type	waiting_query	blocking_account	blocking_lock_type	blocking_lock_duration
test	lock_waits	root@localhost	SHARED_UPGRADABLE	ALTER TABLE test.lock_waits ADD k VARCHAR(20)	root@localhost	SHARED_NO_READ_WRITE	TRANSACTION
SELECT SLEEP(1);
SLEEP(1)
0
SELECT object_schema, object_name,
waiting_account, waiting_lock_type, waiting_query,
blocking_account, blocking_lock_type, blocking_lock_duration
FROM sys.schema_table_lock_waits;
object_schema	object_name	waiting_account	waiting_lock_type	waiting_query	blocking_account	blocking_lock_type	blocking_lock_duration
DROP TABLE test.lock_waits;

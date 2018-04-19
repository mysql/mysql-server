call mtr.add_suppression("This is a warning from test plugin");
call mtr.add_suppression("This is an error from test plugin");
------ Setup ------------------------------------------------
CREATE DATABASE mydb;
------ Run plugin ------------------------------------------------
INSTALL PLUGIN test_sql_cmds_1 SONAME 'TEST_SQL_CMDS_1';
------ Stop plugin -----------------------------------------------
UNINSTALL PLUGIN test_sql_cmds_1;
------ Check output from tested service --------------------------------------
======================================================
COM_QUERY-----------------------------------------------------------------
SELECT id,info FROM information_schema.processlist
-----------------------------------------------------------------
info	
num_cols: 2
server status: 34
warn count: 0
-----------------------------------------------------------------
SELECT * FROM performance_schema.global_variables WHERE variable_name LIKE 'INNODB_READ_IO_THREADS'
-----------------------------------------------------------------
info	
num_cols: 2
server status: 34
warn count: 0
ERROR 1064 You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'garbage' at line 1
error after bad SQL: 1064: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'garbage' at line 1
srv_session_close.
======================================================
current_db before init_db : (null)
current_db after init_db  : mysql
======================================================
COM_KILL
session is dead? 0
session is dead now? 1053
======================================================
test_query_kill
session is dead? 0
Executing select sleep(10)
run KILL QUERY
Killed run_command return value: 0
thread shutdown: 0 (no)
thread error: 0
thread error msg: 
session is dead (after)? 0
OK
======================================================
COM_QUERY with priv
create user as root: 0 
now try as ordinary user
ERROR 1227 Access denied; you need (at least one of) the CREATE USER privilege(s) for this operation
create user supposed to fail: 1227 Access denied; you need (at least one of) the CREATE USER privilege(s) for this operation
drop user as root: 0 
------- Cleanup --------------------------------------
DROP DATABASE mydb;

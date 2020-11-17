call mtr.add_suppression("This is a warning from test plugin");
call mtr.add_suppression("This is an error from test plugin");
------ Setup ------------------------------------------------
------ Run plugin ------------------------------------------------
INSTALL PLUGIN test_sql_reset_connection SONAME 'TEST_SQL_RESET_CONNECTION';
------ Stop plugin -----------------------------------------------
UNINSTALL PLUGIN test_sql_reset_connection;
------ Check output from tested service --------------------------------------
=======================================================================
COM_RESET_CONNECTION
set @secret = 123
select @secret
-----------------------------------------------------------------
@secret LONGLONG(8)	
123
select @secret
-----------------------------------------------------------------
@secret VARCHAR(15)	
[NULL]
Has session ID changed: 0
=======================================================================
COM_RESET_CONNECTION from another session
set @another_secret = 456
select @another_secret
-----------------------------------------------------------------
@another_secret LONGLONG(8)	
456
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
do reset_connection()
COM_RESET_CONNECTION
set @secret = 123
select @secret
-----------------------------------------------------------------
@secret LONGLONG(8)	
123
select @secret
-----------------------------------------------------------------
@secret VARCHAR(15)	
[NULL]
Has session ID changed: 0
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
select @another_secret
-----------------------------------------------------------------
@another_secret LONGLONG(8)	
456
Has session ID changed: 0
=======================================================================
------- Cleanup --------------------------------------

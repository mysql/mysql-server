--source include/have_debug.inc

SET @old_debug = @@GLOBAL.debug;

#
# Bug#34678 @@debug variables incremental mode
#

set debug= 'T';
select @@debug;
set debug= '+P';
select @@debug;
set debug= '-P';
select @@debug;

#
# Bug#38054: "SET SESSION debug" modifies @@global.debug variable
#

SELECT @@session.debug, @@global.debug;

SET SESSION debug = '';

SELECT @@session.debug, @@global.debug;

--echo #
--echo # Bug #52629: memory leak from sys_var_thd_dbug in 
--echo #  binlog.binlog_write_error
--echo #

SET GLOBAL debug='d,injecting_fault_writing';
SELECT @@global.debug;
SET GLOBAL debug='';
SELECT @@global.debug;

SET GLOBAL debug=@old_debug;

--echo #
--echo # Bug #56709: Memory leaks at running the 5.1 test suite
--echo # 

SET @old_local_debug = @@debug;

SET @@debug='d,foo';
SELECT @@debug;
SET @@debug='';
SELECT @@debug;

SET @@debug = @old_local_debug;

--echo End of 5.1 tests


--echo #
--echo # Bug#46165 server crash in dbug
--echo #

SET @old_globaldebug = @@global.debug;
SET @old_sessiondebug= @@session.debug;

--echo # Test 1 - Bug test case, single connection
SET GLOBAL  debug= '+O,../../log/bug46165.1.trace';
SET SESSION debug= '-d:-t:-i';

SET GLOBAL  debug= '';
SET SESSION debug= '';

--echo # Test 2 - Bug test case, two connections
--echo # Connection default
connection default;
SET GLOBAL  debug= '+O,../../log/bug46165.2.trace';
SET SESSION debug= '-d:-t:-i';

--echo # Connection con1
connect (con1, localhost, root);
SET GLOBAL  debug= '';

--echo # Connection default
connection default;
SET SESSION debug= '';
--echo # Connection con1
connection con1;
disconnect con1;
--source include/wait_until_disconnected.inc
--echo # Connection default
connection default;
SET GLOBAL  debug= '';

--echo # Test 3 - Active session trace file on disconnect
--echo # Connection con1
connect (con1, localhost, root);
SET GLOBAL  debug= '+O,../../log/bug46165.3.trace';
SET SESSION debug= '-d:-t:-i';
SET GLOBAL  debug= '';
disconnect con1;
--source include/wait_until_disconnected.inc

--echo # Test 4 - Active session trace file on two connections
--echo # Connection default
connection default;
SET GLOBAL  debug= '+O,../../log/bug46165.4.trace';
SET SESSION debug= '-d:-t:-i';

--echo # Connection con1
connect (con1, localhost, root);
SET SESSION debug= '-d:-t:-i';
SET GLOBAL  debug= '';
SET SESSION debug= '';

--echo # Connection default
connection default;
SET SESSION debug= '';
--echo # Connection con1
connection con1;
disconnect con1;
--source include/wait_until_disconnected.inc
--echo # Connection default
connection default;

--echo # Test 5 - Different trace files
SET SESSION debug= '+O,../../log/bug46165.5.trace';
SET SESSION debug= '+O,../../log/bug46165.6.trace';
SET SESSION debug= '-O';

SET GLOBAL  debug= @old_globaldebug;
SET SESSION debug= @old_sessiondebug;

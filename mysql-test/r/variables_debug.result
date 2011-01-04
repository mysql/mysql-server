SET @old_debug = @@GLOBAL.debug;
set debug= 'T';
select @@debug;
@@debug
T
set debug= '+P';
select @@debug;
@@debug
P:T
set debug= '-P';
select @@debug;
@@debug
T
SELECT @@session.debug, @@global.debug;
@@session.debug	@@global.debug
T	
SET SESSION debug = '';
SELECT @@session.debug, @@global.debug;
@@session.debug	@@global.debug
	
#
# Bug #52629: memory leak from sys_var_thd_dbug in 
#  binlog.binlog_write_error
#
SET GLOBAL debug='d,injecting_fault_writing';
SELECT @@global.debug;
@@global.debug
d,injecting_fault_writing
SET GLOBAL debug='';
SELECT @@global.debug;
@@global.debug

SET GLOBAL debug=@old_debug;
#
# Bug #56709: Memory leaks at running the 5.1 test suite
# 
SET @old_local_debug = @@debug;
SET @@debug='d,foo';
SELECT @@debug;
@@debug
d,foo
SET @@debug='';
SELECT @@debug;
@@debug

SET @@debug = @old_local_debug;
End of 5.1 tests
#
# Bug#46165 server crash in dbug
#
SET @old_globaldebug = @@global.debug;
SET @old_sessiondebug= @@session.debug;
# Test 1 - Bug test case, single connection
SET GLOBAL  debug= '+O,../../log/bug46165.1.trace';
SET SESSION debug= '-d:-t:-i';
SET GLOBAL  debug= '';
SET SESSION debug= '';
# Test 2 - Bug test case, two connections
# Connection default
SET GLOBAL  debug= '+O,../../log/bug46165.2.trace';
SET SESSION debug= '-d:-t:-i';
# Connection con1
SET GLOBAL  debug= '';
# Connection default
SET SESSION debug= '';
# Connection con1
# Connection default
SET GLOBAL  debug= '';
# Test 3 - Active session trace file on disconnect
# Connection con1
SET GLOBAL  debug= '+O,../../log/bug46165.3.trace';
SET SESSION debug= '-d:-t:-i';
SET GLOBAL  debug= '';
# Test 4 - Active session trace file on two connections
# Connection default
SET GLOBAL  debug= '+O,../../log/bug46165.4.trace';
SET SESSION debug= '-d:-t:-i';
# Connection con1
SET SESSION debug= '-d:-t:-i';
SET GLOBAL  debug= '';
SET SESSION debug= '';
# Connection default
SET SESSION debug= '';
# Connection con1
# Connection default
# Test 5 - Different trace files
SET SESSION debug= '+O,../../log/bug46165.5.trace';
SET SESSION debug= '+O,../../log/bug46165.6.trace';
SET SESSION debug= '-O';
SET GLOBAL  debug= @old_globaldebug;
SET SESSION debug= @old_sessiondebug;

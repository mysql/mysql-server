# Include file to decrease test code duplication

--source include/have_debug.inc

call mtr.add_suppression("Can't create thread to");

# save the initial number of concurrent sessions.
--source include/count_sessions.inc

# Save global variables that are going to be used in this connection.
SET @old_debug= @@GLOBAL.debug;
SET @orig_max_connections= @@global.max_connections;
--enable_connect_log

#  Make 3 connects and issue SELECT 1 and then test error
#  ER_CANT_CREATE_THREAD and the do disconnects of the three connections.
connect (con1, localhost, root,,mysql);
SELECT 1;
connect (con2, localhost, root,,mysql);
SELECT 1;
connect (con3, localhost, root,,mysql);
SELECT 1;

# Test ER_CANT_CREATE_THREAD
SET GLOBAL debug= '+d,fail_thread_create';
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
--error ER_CANT_CREATE_THREAD
connect (con4, localhost, root,,mysql);

connection default;
SET GLOBAL debug="-d,fail_thread_create";
disconnect con1;
disconnect con2;
disconnect con3;
--source include/wait_until_count_sessions.inc

# Test various error conditions.

# Test ER_CON_COUNT_ERROR.
SET GLOBAL max_connections= 3;
connect (con1, localhost, root,,mysql);
connect (con2, localhost, root,,mysql);
connect (con3, localhost, root,,mysql);
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
--error ER_CON_COUNT_ERROR
connect (con4, localhost, root,,mysql);

connection default;
disconnect con1;
disconnect con2;
disconnect con3;
--source include/wait_until_count_sessions.inc

# Test resource allocation failure like out of memory.
SET GLOBAL debug= '+d,simulate_resource_failure';
call mtr.add_suppression("Out of memory");
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
--replace_regex /system error: [0-9]+/system error: NUM/
--error 2013 
connect (con1, localhost, root,,mysql);

connection default;
SET GLOBAL debug= '-d,simulate_resource_failure';
SET GLOBAL debug= @old_debug;
SET GLOBAL max_connections= @orig_max_connections;


--echo #
--echo # BUG28508923 - SETTING THREAD_CACHE_SIZE TO 0 DOES NOT EMPTY THE THREAD CACHE.
--echo #

--source include/count_sessions.inc
SET @saved_thread_cache_size=@@thread_cache_size;
SET GLOBAL thread_cache_size=5;

connect(con1, localhost, root,,);
connect(con2, localhost, root,,);
connect(con3, localhost, root,,);
connect(con4, localhost, root,,);
connect(con5, localhost, root,,);
disconnect con1;
disconnect con2;
disconnect con3;
disconnect con4;
disconnect con5;
connection default;
# Wait until Threads_cached value is 5.
let $wait_condition=select count(*)=1 from performance_schema.session_status where VARIABLE_NAME='Threads_cached' and VARIABLE_VALUE=5;
--source include/wait_condition.inc
# Now show shrink the cache size to 2.
SET GLOBAL thread_cache_size = 2;
# Wait until the thread cache size shrinks to 2.
let $wait_condition=select count(*)=1 from performance_schema.session_status where VARIABLE_NAME='Threads_cached' and VARIABLE_VALUE=2;
--source include/wait_condition.inc
SHOW STATUS LIKE 'Threads_cached';
# Now disable the thread cache.
SET GLOBAL thread_cache_size = 0;
# Wait until the thread cache size is 0.
let $wait_condition=select count(*)=1 from performance_schema.session_status where VARIABLE_NAME='Threads_cached' and VARIABLE_VALUE=0;
--source include/wait_condition.inc
SHOW STATUS LIKE 'Threads_cached';

# Privilege check
CREATE USER u1;
--connect(con1, localhost, u1,'',)
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL thread_cache_size = 5;
connection default;
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO u1;
connection con1;
SET GLOBAL thread_cache_size=5;
disconnect con1;
--connection default
DROP USER u1;
# Restore the original value of thread cache size.
SET GLOBAL thread_cache_size=@saved_thread_cache_size;


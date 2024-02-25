#
# BUG28508923 - SETTING THREAD_CACHE_SIZE TO 0 DOES NOT EMPTY THE THREAD CACHE.
#
SET @saved_thread_cache_size=@@thread_cache_size;
SET GLOBAL thread_cache_size=5;
SET GLOBAL thread_cache_size = 2;
SHOW STATUS LIKE 'Threads_cached';
Variable_name	Value
Threads_cached	2
SET GLOBAL thread_cache_size = 0;
SHOW STATUS LIKE 'Threads_cached';
Variable_name	Value
Threads_cached	0
CREATE USER u1;
SET GLOBAL thread_cache_size = 5;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO u1;
SET GLOBAL thread_cache_size=5;
DROP USER u1;
SET GLOBAL thread_cache_size=@saved_thread_cache_size;

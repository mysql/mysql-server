SET @global_thread_cache_size = @@GLOBAL.thread_cache_size;
FLUSH STATUS;
'# Test1#'
SET @@GLOBAL.thread_cache_size=3;
SHOW STATUS LIKE 'Threads_cached';
Variable_name	Value
Threads_cached	0
0 Expected
** Connecting conn1 using username 'root' **
** Connecting conn2 using username 'root' **
** Connecting conn3 using username 'root' **
** Connecting conn4 using username 'root' **
** Connection default **
SHOW STATUS LIKE 'Threads_cached';
Variable_name	Value
Threads_cached	0
0 Expected
** Disconnecting conn1,conn2,conn3,conn4 **
SHOW STATUS LIKE 'Threads_cached';
Variable_name	Value
Threads_cached	3
3 Expected
SET @@GLOBAL.thread_cache_size= 1;
** Connecting conn1 using username 'root' **
** Connecting conn2 using username 'root' **
connection default;
** Disconnecting conn1,conn2 **
SHOW STATUS LIKE 'Threads_cached';
Variable_name	Value
Threads_cached	1
1 Expected
SET @@GLOBAL.thread_cache_size = @global_thread_cache_size;

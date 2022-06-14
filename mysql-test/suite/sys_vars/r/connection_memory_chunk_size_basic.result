SELECT @@global.connection_memory_chunk_size;
@@global.connection_memory_chunk_size
8912
SELECT @@session.connection_memory_chunk_size;
@@session.connection_memory_chunk_size
8912
SET @@global.connection_memory_chunk_size = 25666777;
SELECT @@global.connection_memory_chunk_size;
@@global.connection_memory_chunk_size
25666777
SET @@session.connection_memory_chunk_size = 25666777;
SELECT @@session.connection_memory_chunk_size;
@@session.connection_memory_chunk_size
25666777
SET @@global.connection_memory_chunk_size = -5666777;
Warnings:
Warning	1292	Truncated incorrect connection_memory_chunk_size value: '-5666777'
SELECT @@global.connection_memory_chunk_size;
@@global.connection_memory_chunk_size
1
SET @@session.connection_memory_chunk_size = -5666777;
Warnings:
Warning	1292	Truncated incorrect connection_memory_chunk_size value: '-5666777'
SELECT @@session.connection_memory_chunk_size;
@@session.connection_memory_chunk_size
1
SET @@global.connection_memory_chunk_size = -1;
Warnings:
Warning	1292	Truncated incorrect connection_memory_chunk_size value: '-1'
SELECT @@global.connection_memory_chunk_size;
@@global.connection_memory_chunk_size
1
SET @@session.connection_memory_chunk_size = -1;
Warnings:
Warning	1292	Truncated incorrect connection_memory_chunk_size value: '-1'
SELECT @@session.connection_memory_chunk_size;
@@session.connection_memory_chunk_size
1
SET @@global.connection_memory_chunk_size = 0;
Warnings:
Warning	1292	Truncated incorrect connection_memory_chunk_size value: '0'
SELECT @@global.connection_memory_chunk_size;
@@global.connection_memory_chunk_size
1
SET @@session.connection_memory_chunk_size = 0;
Warnings:
Warning	1292	Truncated incorrect connection_memory_chunk_size value: '0'
SELECT @@session.connection_memory_chunk_size;
@@session.connection_memory_chunk_size
1
SET @@global.connection_memory_chunk_size = default;
SET @@session.connection_memory_chunk_size = default;
SET @@global.connection_memory_chunk_size = NULL;
ERROR 42000: Incorrect argument type to variable 'connection_memory_chunk_size'
SELECT @@global.connection_memory_chunk_size;
@@global.connection_memory_chunk_size
8912
SET @@session.connection_memory_chunk_size = NULL;
ERROR 42000: Incorrect argument type to variable 'connection_memory_chunk_size'
SELECT @@session.connection_memory_chunk_size;
@@session.connection_memory_chunk_size
8912
SET @@global.connection_memory_chunk_size = 'foo';
ERROR 42000: Incorrect argument type to variable 'connection_memory_chunk_size'
SELECT @@global.connection_memory_chunk_size;
@@global.connection_memory_chunk_size
8912
SET @@session.connection_memory_chunk_size = 'foo';
ERROR 42000: Incorrect argument type to variable 'connection_memory_chunk_size'
SELECT @@session.connection_memory_chunk_size;
@@session.connection_memory_chunk_size
8912

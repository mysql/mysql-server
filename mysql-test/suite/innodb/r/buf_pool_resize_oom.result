#
# Bug #21348684 SIGABRT DURING RESIZING THE INNODB BUFFER POOL
# ONLINE WITH MEMORY FULL CONDITION
#
SET GLOBAL DEBUG='+d,buf_pool_resize_chunk_null';
# Setting innodb buffer pool size 10MB more than before
SET GLOBAL innodb_buffer_pool_size=@before_innodb_buffer_pool_size + 10485760;
Warnings:
Warning	1292	Truncated incorrect innodb_buffer_pool_size value: '1084227584'
SET GLOBAL DEBUG='-d,buf_pool_resize_chunk_null';
SET GLOBAL innodb_buffer_pool_size=@before_innodb_buffer_pool_size;

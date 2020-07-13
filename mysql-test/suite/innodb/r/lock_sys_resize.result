# Bug #31329634 ASSERTION FAILURE:
#    LOCK0LATCHES.CC:42:LOCK_SYS->REC_HASH->N_CELLS == LOCK_SYS->P
SELECT @@innodb_buffer_pool_size;
@@innodb_buffer_pool_size
17825792
SELECT @@innodb_buffer_pool_chunk_size;
@@innodb_buffer_pool_chunk_size
1048576
CREATE TABLE t1 (id INT PRIMARY KEY, val VARCHAR(1000)) ENGINE=INNODB;
INSERT INTO t1 (id,val) VALUES (1,''),(2,''),(3,''),(4,''),(5,'');
SET DEBUG_SYNC='lock_rec_restore_from_page_infimum_will_latch
    SIGNAL con1_will_latch
    WAIT_FOR con1_can_go';
UPDATE t1 SET val='aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa';
SET DEBUG_SYNC='now WAIT_FOR con1_will_latch';
SET GLOBAL DEBUG='+d,syncpoint_after_lock_sys_resize_rec_hash';
SET GLOBAL innodb_buffer_pool_size=
@@innodb_buffer_pool_size * 2 + @@innodb_buffer_pool_chunk_size;
SET DEBUG_SYNC='now WAIT_FOR reached_after_lock_sys_resize_rec_hash';
SET DEBUG_SYNC='now SIGNAL con1_can_go';
SET GLOBAL DEBUG='-d,syncpoint_after_lock_sys_resize_rec_hash';
SET DEBUG_SYNC='now SIGNAL continue_after_lock_sys_resize_rec_hash';
DROP TABLE t1;
SELECT @@innodb_buffer_pool_size;
@@innodb_buffer_pool_size
36700160
SET GLOBAL innodb_buffer_pool_size=
(@@innodb_buffer_pool_size - @@innodb_buffer_pool_chunk_size) div 2;
SELECT @@innodb_buffer_pool_size;
@@innodb_buffer_pool_size
17825792

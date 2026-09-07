#
# Create and drop a table, larger than the buffer pool so that
# everything else is evicted from it.
#
# Beware: Running this with big buffer pool is slow.
#

-- disable_query_log

-- echo # Emptying InnoDB buffer pool: begin

-- let $bp_size = `SELECT @@innodb_buffer_pool_size`
-- source suite/innodb/include/evict_everything_from_buffer_pool_prepare.inc
-- echo # Buffer pool size: $bp_size bytes
-- echo # Creating and dropping a table with size: $table_size bytes
-- source suite/innodb/include/evict_everything_from_buffer_pool_evict.inc
-- source suite/innodb/include/evict_everything_from_buffer_pool_cleanup.inc

-- echo # Emptying InnoDB buffer pool: end

-- enable_query_log

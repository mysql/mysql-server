#
# Create a table larger than the buffer pool so that scanning it later evicts
# other pages from the buffer pool.
#
# Beware: Running this with big buffer pool is slow.
#

-- disable_warnings
DROP TEMPORARY TABLE IF EXISTS _big;
-- enable_warnings
CREATE TEMPORARY TABLE _big (a TEXT) ENGINE=INNODB;
-- let $bp_size = `SELECT @@innodb_buffer_pool_size`
-- let $table_size = `SELECT ROUND($bp_size * 2.0)`
-- let $row_size = 1024
-- let $i = `SELECT ROUND($table_size / $row_size)`
BEGIN;
while ($i)
{
        -- eval INSERT INTO _big VALUES (REPEAT('a', $row_size));
        dec $i;
}
COMMIT;

# Keep the temporary table attached to this connection after population. Some
# tests scan it while another thread is paused inside InnoDB with dictionary
# state held, when opening a normal table could block before eviction starts.

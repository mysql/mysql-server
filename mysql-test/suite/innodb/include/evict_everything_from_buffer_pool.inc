#
# Create and drop a table, larger than the buffer pool so that
# everything else is evicted from it.
#
# Beware: Running this with big buffer pool is slow.
#

-- disable_query_log

-- echo # Emptying InnoDB buffer pool: begin

CREATE TABLE _big (a TEXT) ENGINE=INNODB;
-- let $bp_size = `SELECT @@innodb_buffer_pool_size`
-- echo # Buffer pool size: $bp_size bytes
-- let $table_size = `SELECT ROUND($bp_size * 2.0)`
-- echo # Creating and dropping a table with size: $table_size bytes
-- let $row_size = 1024
-- let $i = `SELECT ROUND($table_size / $row_size)`
BEGIN;
while ($i)
{
        -- eval INSERT INTO _big VALUES (REPEAT('a', $row_size));
        dec $i;
}
COMMIT;
-- disable_result_log
SELECT * FROM _big WHERE a = 'find nothing';
-- enable_result_log
DROP TABLE _big;

-- echo # Emptying InnoDB buffer pool: end

-- enable_query_log

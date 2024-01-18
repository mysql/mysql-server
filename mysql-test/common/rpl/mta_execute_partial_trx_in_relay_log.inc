# ==== Implementation ====
#
# 1. Set debug point to rotate relay log after table_map_event.
# 2. Wait for io thread to receive table_map_event and stop it.
# 3. Start the io thread after it is stopped.
# 4. Check that there is no failure on replication.
#

# Initial setup
--source include/rpl/connection_source.inc
CREATE TABLE t1(i INT) ENGINE=InnoDB;
FLUSH LOGS;
--source include/rpl/sync_to_replica.inc

# Set debug point to rotate relay log after table_map_event.
SET @save_debug=@@global.debug;
SET GLOBAL DEBUG='d,stop_io_after_reading_table_map_event';
--source include/rpl/connection_source.inc
INSERT INTO t1 VALUES (1);

--source include/rpl/connection_replica.inc
# Wait for io thread to receive table_map_event and stop it.
--source include/rpl/wait_for_receiver_to_stop.inc
SET GLOBAL DEBUG= @save_debug;

# Start the io thread after it is stopped.
--source include/rpl/start_receiver.inc

# Check that their is no failure on replication.
--source include/rpl/connection_source.inc
--source include/rpl/sync_to_replica.inc

# Cleanup
--source include/rpl/connection_source.inc
DROP TABLE t1;
--source include/rpl/deinit.inc

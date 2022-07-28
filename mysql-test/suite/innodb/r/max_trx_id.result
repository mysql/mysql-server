# Initialize new data directory and restart MySQL on that data directory.
# The new data directory will be initialized by instance which has:
# --debug="d,trx_sys_low_trx_id_write_margin" so it will do reservations
# of trx_id values every 8 trx_id values (instead of every 256 values).
# This way we could collect enough connections in mtr test runner to fill
# concurrently the reservation interval (limit for mtr connections is 128).
# Restart MySQL using the new data directory.
# restart: --datadir=tmp/max_trx_id --log-error=my_restart.err --debug=d,trx_sys_low_trx_id_write_margin --innodb-redo-log-capacity=8M --innodb_page_size=INNODB_PAGE_SIZE
# Create 7 connections which keep creating and comitting transactions.
# Pause the created connections before trx_sys_mutex_enter() is called
# before assigning trx->id.
SET DEBUG_SYNC = "trx_sys_before_assign_id SIGNAL con_id_waiting1 WAIT_FOR con_go NO_CLEAR_EVENT";
SET @id = 1;
START TRANSACTION;
UPDATE t SET c = c+1 WHERE a = @id;
SET DEBUG_SYNC = "trx_sys_before_assign_id SIGNAL con_id_waiting2 WAIT_FOR con_go NO_CLEAR_EVENT";
SET @id = 2;
START TRANSACTION;
UPDATE t SET c = c+1 WHERE a = @id;
SET DEBUG_SYNC = "trx_sys_before_assign_id SIGNAL con_id_waiting3 WAIT_FOR con_go NO_CLEAR_EVENT";
SET @id = 3;
START TRANSACTION;
UPDATE t SET c = c+1 WHERE a = @id;
SET DEBUG_SYNC = "trx_sys_before_assign_id SIGNAL con_id_waiting4 WAIT_FOR con_go NO_CLEAR_EVENT";
SET @id = 4;
START TRANSACTION;
UPDATE t SET c = c+1 WHERE a = @id;
SET DEBUG_SYNC = "trx_sys_before_assign_id SIGNAL con_id_waiting5 WAIT_FOR con_go NO_CLEAR_EVENT";
SET @id = 5;
START TRANSACTION;
UPDATE t SET c = c+1 WHERE a = @id;
SET DEBUG_SYNC = "trx_sys_before_assign_id SIGNAL con_id_waiting6 WAIT_FOR con_go NO_CLEAR_EVENT";
SET @id = 6;
START TRANSACTION;
UPDATE t SET c = c+1 WHERE a = @id;
SET DEBUG_SYNC = "trx_sys_before_assign_id SIGNAL con_id_waiting7 WAIT_FOR con_go NO_CLEAR_EVENT";
SET @id = 7;
START TRANSACTION;
UPDATE t SET c = c+1 WHERE a = @id;
# Wait.
# Wait for 1....
SET DEBUG_SYNC = "now WAIT_FOR con_id_waiting1";
# Wait for 2....
SET DEBUG_SYNC = "now WAIT_FOR con_id_waiting2";
# Wait for 3....
SET DEBUG_SYNC = "now WAIT_FOR con_id_waiting3";
# Wait for 4....
SET DEBUG_SYNC = "now WAIT_FOR con_id_waiting4";
# Wait for 5....
SET DEBUG_SYNC = "now WAIT_FOR con_id_waiting5";
# Wait for 6....
SET DEBUG_SYNC = "now WAIT_FOR con_id_waiting6";
# Wait for 7....
SET DEBUG_SYNC = "now WAIT_FOR con_id_waiting7";
# Create a connection which will hang inside trx_sys_write_max_trx_id()
# when holding the trx_sys_serialisation_mutex.
SET DEBUG_SYNC = "trx_sys_write_max_trx_id__ser SIGNAL write_waiting WAIT_FOR write_go";
CALL trx_loop(-1);
# Wait until the created connection is hanging.
SET DEBUG_SYNC = "now WAIT_FOR write_waiting";
# Let the paused connections go, to check if they could corrupt the database.
# Do not allow them to write max_trx_id to the transaction system header page.
SET GLOBAL DEBUG = "+d,trx_sys_write_max_trx_id__all_blocked";
SET DEBUG_SYNC = "now SIGNAL con_go";
# Wait a little bit, giving a chance to corrupt the database to the resumed
# connections (by writing down too high trx_id to some page / row).
# Kill and restart MySQL to see if the database is not corrupted.
# Kill and restart: --datadir=tmp/max_trx_id --log-error=my_restart.err --debug=d,trx_sys_low_trx_id_write_margin --innodb-redo-log-capacity=8M --innodb_page_size=INNODB_PAGE_SIZE
# Update rows using a new transaction. It would crash if the transaction
# received a trx->id which had been assigned to some other transaction
# and written to some page/row before MySQL crashed.
UPDATE t SET c = c + 1;
# Check if there is no corruption if MySQL instantly crashed after using
# the new transaction (after restart).
# Kill and restart: --datadir=tmp/max_trx_id --log-error=my_restart.err --debug=d,trx_sys_low_trx_id_write_margin --innodb-redo-log-capacity=8M --innodb_page_size=INNODB_PAGE_SIZE
# Update all rows using a new transaction to see if there is no assertion
# failure on trx_id for any of the related pages.
UPDATE t SET c = c + 1;
# Turn off MySQL and remove its data directory.
# Kill the server
# Restart usual instance of MySQL which is used for mtr tests.
# restart:

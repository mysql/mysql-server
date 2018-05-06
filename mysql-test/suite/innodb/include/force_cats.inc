# Current implementation of InnoDB uses CATS whenever
# the number of waiting transactions exceeds LOCK_CATS_THRESHOLD
# so a relatively non-intrusive way to force CATS
# is to create 1+LOCK_CATS_THRESHOLD connections:
# one holding an exclusive lock, and others waiting for it.
# This is what this script does.
#
# There is another script, discourage_cats.inc
# which kills the connections created by this script,
# and thus gives the InnoDB chance to go back to FCFS mode.

--disable_query_log
--disable_result_log
--source ./cats_config.inc

#  Prepare an object that we can wait for in InnoDB
#  lock system: a row in an InnoDB table.

  --connection default
  CREATE TABLE cats (id int PRIMARY KEY) Engine=InnoDB;
  INSERT INTO cats (id) VALUES (1);


# Creating 1 + LOCK_CATS_THRESHOLD threads named cats0...$LOCK_CATS_THRESHOLD:
#   cats0 will obtain X lock on cats only row
#   cats1...cats$LOCK_CATS_THRESHOLD will wait for the lock

# cats0 connects and obtains the X lock

  --echo # establishing connection cats0
  --connect (cats0,localhost, root,,)
  SET innodb_lock_wait_timeout = 1000000;
  SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
  BEGIN;
  SELECT id FROM cats FOR UPDATE;

# cats1...cats$LOCK_CATS_THRESHOLD connect and wait for an S lock

  --let $cats_connection_number = 1
  while($cats_connection_number <= $LOCK_CATS_THRESHOLD)
  {
    --let $cats_connection_name = cats$cats_connection_number
    --echo # establishing connection $cats_connection_name
    --connect ($cats_connection_name, localhost, root,,)
    SET innodb_lock_wait_timeout = 1000000;
    SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
    BEGIN;
    --eval SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL $cats_connection_name'
    --send SELECT id FROM cats FOR SHARE

    --inc $cats_connection_number
  }

# wait for cats1...cats$LOCK_CATS_THRESHOLD to enter wait queue

  --connection default
  --let $cats_connection_number = 1
  while($cats_connection_number <= $LOCK_CATS_THRESHOLD)
  {
    --let $cats_connection_name = cats$cats_connection_number
    --echo # wating for $cats_connection_name
    --eval SET DEBUG_SYNC = 'now WAIT_FOR $cats_connection_name'

    --inc $cats_connection_number
  }
--enable_result_log
--enable_query_log

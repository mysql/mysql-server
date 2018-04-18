# Scenario "random" - both redo and no-redo mini transactions happen
# concurrently in many threads. We freeze/unfreeze recent_closed
# updates every 1 second (trying to make troubles).
#
# We rely on assertion for proper order in flush list.
#

--echo #
--echo # 0. Freeze log.recent_closed.
--echo #

--source ../include/log_recent_closed_freeze.inc

--echo #
--echo # 1. Gather threads that try to insert dirtied pages by redo-based mtr.
--echo #

let $i = $n_conn;
while ($i > 0) {
  --echo Connection $i starts inserting rows...

  connect(con_redo$i, localhost, root,,);
  SET DEBUG_SYNC = "mtr_redo_before_add_dirty_blocks WAIT_FOR mtr_random_go NO_CLEAR_EVENT";

  eval SET @q = CONCAT("CALL mtr_redo_generate_multi(", $i, ", 10000, 100)");
  PREPARE stmt FROM @q;
  --send EXECUTE stmt;

  dec $i;
}
--connection default

--echo #
--echo # 2. Gather threads that try to insert dirtied pages by noredo-based mtr.
--echo #

let $i = $n_conn;
while ($i > 0) {
  connect(con_noredo$i, localhost, root,,);
  CREATE TEMPORARY TABLE tmp_t (a INT PRIMARY KEY, b VARCHAR(10000)) ENGINE = InnoDB;
  INSERT INTO tmp_t (a, b) VALUES (0, '');
  dec $i;
}

let $i = $n_conn;
while ($i > 0) {
  --echo Connection $i starts inserting rows...
  --connection con_noredo$i

  SET DEBUG_SYNC = CONCAT("mtr_noredo_before_add_dirty_blocks ",
                          "WAIT_FOR mtr_random_go NO_CLEAR_EVENT");

  --send CALL mtr_noredo_generate_multi(100);

  dec $i;
}
--connection default

--echo #
--echo # 3. Start the gathered threads.
--echo #

SET DEBUG_SYNC = "now SIGNAL mtr_random_go";

--echo #
--echo # 4. Freeze/unfreeze log_recent_closed every 1s
--echo #

let $i = 10;
while ($i) {
  if ($debug_test) {
    --source ../include/log_read_current_lsn.inc
    --echo Current LSN: $current_lsn
  }
  --echo Iterations left: $i - unfreeze...
  --source ../include/log_recent_closed_unfreeze.inc
  real_sleep 0.5;
  --echo Iterations left: $i - freeze...
  --source ../include/log_recent_closed_freeze.inc
  real_sleep 0.5;
  dec $i;
}

--echo Finished all iterations - unfreeze...
--source ../include/log_recent_closed_unfreeze.inc

--echo #
--echo # 5. Finish pending insertions.
--echo #

let $i = $n_conn;
while ($i > 0) {
  --connection con_redo$i
  reap;
  disconnect con_redo$i;
  --connection con_noredo$i
  reap;
  disconnect con_noredo$i;
  dec $i;
}
--connection default

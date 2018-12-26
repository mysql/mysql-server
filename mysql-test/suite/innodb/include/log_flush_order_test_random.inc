# Scenario "random" - both redo and no-redo mini transactions happen
# concurrently in many threads. We freeze/unfreeze recent_closed
# updates every 1 second (trying to make troubles).
#
# We rely on assertion for proper order in flush list.
#

let $i = $n_conn;
while ($i > 0) {
  connect(con_noredo$i, localhost, root,,);

  CREATE TEMPORARY TABLE tmp_t (a INT PRIMARY KEY, b VARCHAR(10000)) ENGINE = InnoDB;
  INSERT INTO tmp_t (a, b) VALUES (0, '');

  connect(con_redo$i, localhost, root,,);

  dec $i;
}

--connection default

--echo #
--echo # 0. Freeze log.recent_closed.
--echo #

--source ../include/log_recent_closed_freeze.inc

--echo #
--echo # 1. Start threads doing redo-based mtr and threads doing noredo-based mtr.
--echo #

let $i = $n_conn;
while ($i > 0) {
  --connection con_redo$i
  BEGIN;
  eval DELETE FROM t$i WHERE a < 0;

  --connection con_noredo$i
  BEGIN;
  DELETE FROM tmp_t WHERE a < 0;

  dec $i;
}

let $i = $n_conn;
while ($i > 0) {
  --connection con_redo$i
  eval SET @q = CONCAT("CALL mtr_redo_generate_multi(", $i, ", 1800, 500)");
  PREPARE stmt FROM @q;
  --send EXECUTE stmt;

  --connection con_noredo$i
  --send CALL mtr_noredo_generate_multi(100);

  dec $i;
}
--connection default

--echo #
--echo # 2. Freeze/unfreeze log_recent_closed every 1s
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
--echo # 3. Finish pending insertions.
--echo #

let $i = $n_conn;
while ($i > 0) {
  --connection con_redo$i
  reap;
  COMMIT;
  disconnect con_redo$i;
  --connection con_noredo$i
  reap;
  COMMIT;
  disconnect con_noredo$i;
  dec $i;
}
--connection default

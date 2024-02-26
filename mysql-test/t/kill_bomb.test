# This test tries to shutdown the server in different ways and remove the
# datadir right away to check if the process is fully closed.
 
--let ORIG=$MYSQL_TMP_DIR/datadir_orig
--let WORK_COPY=$MYSQL_TMP_DIR/datadir_work
--let MYSQLD_DATADIR=`select @@datadir`

CREATE TABLE t3 (a INT PRIMARY KEY);

SET GLOBAL innodb_fast_shutdown = 0;
--source include/shutdown_mysqld.inc
--source include/wait_until_disconnected.inc

--let $restart_parameters = "restart: --datadir=$WORK_COPY"
--force-cpdir $MYSQLD_DATADIR $ORIG
--force-cpdir $ORIG $WORK_COPY

--replace_result $WORK_COPY WORK_COPY
--source include/start_mysqld.inc

# We test three .inc scripts that are used to kill the server.
# They are `stop_mysqld.inc`, `kill_mysqld.inc` and
# `kill_and_restart_mysqld.inc`.
--let $script = 1
while($script <= 3){
  # We test two scenarios of deleting the files. One is trying to first delete
  # some files that we know were opened and modified. Second just tries to
  # delete the whole dir in one operation as soon as possible.
  --let $scenario = 1
  while($scenario <= 2){
    --echo ### Executing scenario $scenario, script $script
    
    # We drop and recreate the table to force the `mysql.ibd` file to be opened
    # and modified. This should additionally trigger dblwr files to be used.
    DROP TABLE t3;
    CREATE TABLE t3 (a INT PRIMARY KEY);
    # Insert some data so the undo log is being written to also.
    INSERT INTO t3 VALUES (1), (4) ,(5);

    # The reason for adding these SELECTs is forgotten. But they increase the
    # failure rate twice.
    --disable_result_log
    SELECT * FROM INFORMATION_SCHEMA.FILES;
    SELECT * FROM INFORMATION_SCHEMA.INNODB_TABLESPACES;
    --enable_result_log
    if ($script == 1) {
      --source include/stop_mysqld.inc
    }
    if ($script == 2) {
      --source include/kill_mysqld.inc
    }
    if ($script == 3) {
      --replace_result $WORK_COPY WORK_COPY
      --source include/kill_and_restart_mysqld.inc
    }
    # The third script is starting the server immediatelly for us. We don't have
    # to do anything, we don't test the files removal. In case the previous
    # process did not fully die, the new one started right away could get some
    # sharing violation errors.
    if ($script != 3) {
      if ($scenario == 1) {
        --remove_file $WORK_COPY/mysql.ibd
        --remove_file $WORK_COPY/#ib_16384_0.dblwr
        --remove_file $WORK_COPY/undo_001
      }
      # Remove the workdir as soon as possible, and recreate it from backup.
      # If the server is not fully dead by now, this will report an error.
      --force-rmdir $WORK_COPY
      --force-cpdir $ORIG $WORK_COPY

      --replace_result $WORK_COPY WORK_COPY
      --source include/start_mysqld.inc
   }

    --inc $scenario
  }
  --inc $script
}

# Cleanup

--let $restart_parameters = "restart:"
--source include/kill_and_restart_mysqld.inc

# Cleanup
--force-rmdir $WORK_COPY
--force-rmdir $ORIG
DROP TABLE t3;

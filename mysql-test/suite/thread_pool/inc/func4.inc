### 0. $worker_count must be so big that we can distinct the
#      thread pool groups
if($worker_count < 3)
{
   --enable_query_log
   --enable_result_log
   --echo # ERROR: worker_count = $worker_count but must be >= 3
   --echo # abort
   exit;
}

### 1. Actualize mysqltest.book_keeping
--disable_query_log
--disable_result_log
if($DEBUG)
{
   --enable_query_log
   --enable_result_log
}
TRUNCATE mysqltest.book_keeping;
TRUNCATE mysqltest.start_statistics;
let $num= $worker_count;
while($num)
{
   eval INSERT INTO mysqltest.book_keeping
   SET worker_id = $num,
       session_id = NULL,
       worker_type = NULL,
       counter = NULL,
       mysql_errno = NULL;
   dec $num;
}

eval UPDATE mysqltest.book_keeping SET
   worker_type = 'first',
   high_priority_connection = @first_high_priority_connection,
   task_size = @first_size, task_transaction = @first_transaction,
   task_prepared = @first_prepared
WHERE $first;
eval UPDATE mysqltest.book_keeping SET
   worker_type = 'second',
   high_priority_connection = @second_high_priority_connection,
   task_size = @second_size, task_transaction = @second_transaction,
   task_prepared = @second_prepared
WHERE NOT $first;
if($DEBUG)
{
   SELECT * FROM mysqltest.book_keeping ORDER BY worker_id;
   SELECT * FROM information_schema.processlist WHERE DB = 'mysqltest' ORDER BY ID;
}
# Check consistency of mysqltest.book_keeping
if(`SELECT COUNT(*) <> 0 FROM mysqltest.book_keeping
     WHERE session_id IS NOT NULL
        OR counter IS NOT NULL
        OR mysql_errno IS NOT NULL`)
{
   --enable_query_log
   --enable_result_log
   --echo # ERROR: Content of mysqltest.book_keeping is inconsistent
   SELECT * FROM mysqltest.book_keeping
   WHERE session_id IS NOT NULL
      OR counter IS NOT NULL
      OR mysql_errno IS NOT NULL
   ORDER BY worker_id;
   --echo # abort
   exit;
}
if(`SELECT COUNT(*) = 0 FROM mysqltest.book_keeping`)
{
   --echo # ERROR: mysqltest.book_keeping must be not empty
   SELECT * FROM mysqltest.book_keeping;
   --echo # abort
   exit;
}


--connection locker
LOCK TABLES mysqltest.to_lock WRITE;
--connection default

### 2. Start the worker sessions
let $worker_script= $MYSQL_TEST_DIR/suite/thread_pool/inc/worker.test;
let $current_id= `SELECT MIN(worker_id) FROM mysqltest.book_keeping
                  WHERE session_id IS NULL`;
let $its_windows= `SELECT CONVERT(@@version_compile_os USING latin1) IN ("Win32","Win64","Windows") = 1`;
let $num= `SELECT COUNT(worker_id) FROM mysqltest.book_keeping`;
while($num)
{
   let $worker_stdout=   $MYSQLTEST_VARDIR/tmp/$current_id.out;
   let $worker_stderr=   $MYSQLTEST_VARDIR/tmp/$current_id.err;

   if (!$its_windows)
   {
      --exec $MYSQL_TEST --test-file=$worker_script > $worker_stdout 2>$worker_stderr &
   }
   if ($its_windows)
   {
      # When calling "--exec ... &" in Windows the parent seems to wait for the child to exit.
      # Therefore we have to go with a different solution for this OS.
      let MYSQLTEST_WORKER_SCRIPT = $worker_script;
      let MYSQLTEST_WORKER_STDOUT = $worker_stdout;
      let MYSQLTEST_WORKER_STDERR = $worker_stderr;
      let MYSQLTEST_BIN = $MYSQL_TEST;
      --perl
      my $worker_script = $ENV{'MYSQLTEST_WORKER_SCRIPT'};
      my $worker_stdout = $ENV{'MYSQLTEST_WORKER_STDOUT'};
      my $worker_stderr = $ENV{'MYSQLTEST_WORKER_STDERR'};
      my $mysqltest_bin = $ENV{'MYSQLTEST_BIN'};
      $SIG{CHLD} = 'IGNORE';
      my $pid = fork();
      if (not defined $pid)
      {
         die "forking of a new mysqltest client failed";
      }
      # Perl manual
      # Process | sees in $pid
      # ---------------------------------
      # parent  | process id of the child
      # child   | 0
      #
      # On Linux we would let the child ($pid ==0) do the exec whereas the parent would not wait
      # till the child disappears. This doesn't work on WIN where the parent would wait.
      # Here we let the parent run the exec whereas the child can go forward without waiting.
      if (defined $pid && $pid != 0 )
      {
         open(STDOUT,">", "$worker_stdout") || die "Can't redirect stdout to '$worker_stdout'";
         open(STDERR,">", "$worker_stderr") || die "Can't redirect stderr to '$worker_stderr'";
         # 1 -> Force a flush on every write or print.
         $| = 1;
         my @mysqltest_with_args = split(' ', $mysqltest_bin);
         push @mysqltest_with_args, "-x";
         push @mysqltest_with_args, "$worker_script";
         exec(@mysqltest_with_args);
         # exec will not return therefore the next line shouldn't be reached.
         print STDERR "exec of (@mysqltest_with_args) failed with: $! though it shouldn't return at all\n";
         print STDERR "(see $worker_stderr and $worker_stdout)\n";
         exit;
      }
      EOF
   }

   # Wait till the worker session shows up in mysqltest.book_keeping
   let $wait_timeout= 10;
   let $wait_condition=
   SELECT COUNT(*) = 1 FROM mysqltest.book_keeping
   WHERE worker_id = $current_id AND session_id IS NOT NULL;
   --source include/wait_condition.inc
   if(!$success)
   {
      --enable_query_log
      --enable_result_log
      --echo # ERROR: Worker session did not show up like expected
      --echo #        session_id must be no more NULL
      eval SELECT * FROM mysqltest.book_keeping
      WHERE worker_id = $current_id;
      SELECT * FROM information_schema.processlist
      WHERE DB = 'mysqltest'
      ORDER BY ID;
      --echo # Sorry have to abort
      exit;
   }
   # We have two thread_groups and all workers must be within the same.
   # The next connection will be send into the other thread_group.
   # Make some connect + disconnect.
   --connect (dummy, localhost, root,,)
   --disconnect dummy
   --connection default
   let $current_id= `SELECT MIN(worker_id) FROM mysqltest.book_keeping
                     WHERE session_id IS NULL`;
   # empty -> SELECT gave NULL -> we have started all required worker threads
   # In case we would go with this value into the while loop again than we
   # would be in trouble. But $num will be decremented and become 0.

   dec $num;
}
--disable_query_log
--disable_result_log
if($DEBUG)
{
   --enable_query_log
   --enable_result_log
   SELECT * FROM information_schema.processlist WHERE DB = 'mysqltest' ORDER BY ID;
   SELECT * FROM mysqltest.book_keeping ORDER BY worker_id;
}

# Wait till all worker sessions are waiting for the MDL lock on mysqltest.to_lock.
# If the workers reach this point than we can be sure that they have set their
# high_priority_connection value.
let $wait_condition=
SELECT COUNT(*) = $worker_count FROM information_schema.processlist
WHERE STATE = 'Waiting for table metadata lock'
  AND DB = 'mysqltest';
--source include/wait_condition.inc
if(!$success)
{
   --enable_query_log
   --enable_result_log
   --echo # ERROR: Not all worker sessions are waiting for the MDL lock
   SELECT * FROM information_schema.processlist
   WHERE DB = 'mysqltest'
   ORDER BY ID;
   SELECT * FROM mysqltest.book_keeping
   ORDER BY worker_id;
  --echo # Sorry have to abort
  exit;
}

--disable_query_log
--disable_result_log
if($DEBUG)
{
   --enable_query_log
   --enable_result_log
   SELECT * FROM information_schema.processlist WHERE DB = 'mysqltest' ORDER BY ID;
   SELECT * FROM mysqltest.book_keeping ORDER BY worker_id;
}

# Preserve the current counters from performance_schema.tp_thread_group_stats
eval INSERT INTO mysqltest.start_statistics (WHAT,$statistics_column_list)
SELECT 'before',$statistics_column_list
FROM performance_schema.tp_thread_group_stats
WHERE CONNECTIONS_STARTED - CONNECTIONS_CLOSED = $worker_count;

--disable_query_log
--disable_result_log
if($DEBUG)
{
   --enable_query_log
   --enable_result_log
   SELECT * FROM mysqltest.start_statistics;
}

### 3. Start the race.
--connection locker
UNLOCK TABLES;

--connection default
let $num= $run_time;
while($num)
{
   --sleep 1
   dec $num;
}
--disable_query_log
--disable_result_log
if($DEBUG)
{
   --enable_query_log
   --enable_result_log
   SELECT * FROM information_schema.processlist WHERE DB = 'mysqltest' ORDER BY ID;
}

### 4. Stop the race.
--connection locker
LOCK TABLES mysqltest.to_lock WRITE;

--connection default
# Wait till all worker sessions are waiting for the MDL lock on mysqltest.to_lock.
let $wait_timeout= 20;
let $wait_condition=
SELECT COUNT(*) = $worker_count FROM information_schema.processlist
WHERE STATE = 'Waiting for table metadata lock'
  AND DB = 'mysqltest';
--source include/wait_condition.inc
if(!$success)
{
   --enable_query_log
   --enable_result_log
   --echo # ERROR: Not all worker sessions are again waiting for the MDL lock
   SELECT * FROM information_schema.processlist
   WHERE DB = 'mysqltest'
   ORDER BY ID;
  --echo # Sorry have to abort
  exit;
}
# include/wait_condition.inc above switched protocolling on
--disable_query_log
--disable_result_log
if($DEBUG)
{
   --enable_query_log
   --enable_result_log
   SELECT * FROM information_schema.processlist WHERE DB = 'mysqltest' ORDER BY ID;
}


# Preserve the current counters from performance_schema.tp_thread_group_stats
eval INSERT INTO mysqltest.start_statistics (WHAT,$statistics_column_list)
SELECT 'after',$statistics_column_list
FROM performance_schema.tp_thread_group_stats
WHERE CONNECTIONS_STARTED - CONNECTIONS_CLOSED = $worker_count;
--disable_query_log
--disable_result_log
if($DEBUG)
{
   --enable_query_log
   --enable_result_log
   SELECT * FROM mysqltest.start_statistics;
}


### 5. Signal the worker sessions to terminate
#   5.1 Kill the current queries of the worker sessions
--disable_query_log
--disable_result_log
if($DEBUG)
{
   --enable_query_log
   --enable_result_log
}

SELECT * FROM information_schema.processlist WHERE DB = 'mysqltest' ORDER BY ID;
SELECT MIN(worker_id) INTO @kill_worker_id FROM mysqltest.book_keeping;
let $num= $worker_count;
while($num)
{
   SELECT session_id into @kill_id FROM mysqltest.book_keeping
   WHERE worker_id = @kill_worker_id;
   KILL QUERY @kill_id;
   SET @kill_worker_id = @kill_worker_id + 1;
   dec $num;
}

--disable_query_log
--disable_result_log
if($DEBUG)
{
   --enable_query_log
   --enable_result_log
   SELECT * FROM information_schema.processlist WHERE DB = 'mysqltest' ORDER BY ID;
   SELECT * FROM mysqltest.book_keeping ORDER BY worker_id;
}

#   5.2 Wait till all worker sessions have disappeared from information_schema.processlist
let $wait_condition=
SELECT COUNT(*) = 0 FROM information_schema.processlist
WHERE DB = 'mysqltest';
--source include/wait_condition.inc
if(!$success)
{
   --enable_query_log
   --enable_result_log
   --echo # ERROR: Not all worker sessions disappeared from information_schema.processlist
   SELECT * FROM information_schema.processlist
   WHERE DB = 'mysqltest';
  --echo # Sorry have to abort
  exit;
}
--disable_query_log
--disable_result_log
if($DEBUG)
{
   --enable_query_log
   --enable_result_log
   SELECT * FROM information_schema.processlist WHERE DB = 'mysqltest' ORDER BY ID;
}

--connection locker
UNLOCK TABLES;

--connection default

--disable_query_log
--disable_result_log
if($DEBUG)
{
   --enable_query_log
   --enable_result_log
   SELECT * FROM mysqltest.book_keeping ORDER BY worker_id;
}

### 6. Some checks
#--------------------
#   6.1 Check that all worker sessions had a proper end
if(`SELECT COUNT(*) > 0 FROM mysqltest.book_keeping
     WHERE counter IS NULL
        OR mysql_errno <> $ER_QUERY_INTERRUPTED OR mysql_errno IS NULL`)
{
   --enable_query_log
   --enable_result_log
   --echo # ERROR in Check 1:
   --echo # All worker sessions must have updated their records (counter,mysql_errno) and
   --echo # mysql_errno must be all time $ER_QUERY_INTERRUPTED (ER_QUERY_INTERRUPTED)
   eval SELECT * FROM mysqltest.book_keeping
   WHERE counter IS NULL
      OR mysql_errno <> $ER_QUERY_INTERRUPTED OR mysql_errno IS NULL
   ORDER BY worker_id;
   --echo # abort
   exit;
}
#   6.2 Check that the priorization system worked sufficient fair
if(`SELECT MIN(counter)/AVG(counter) < 0.5 OR MAX(counter)/AVG(counter) > 2
     FROM mysqltest.book_keeping
     GROUP BY high_priority_connection,task_size,task_transaction,task_prepared
     HAVING COUNT(*) > 1 AND AVG(counter) >= 10`)
{
   --enable_query_log
   --enable_result_log
   --echo # ERROR in Check 2:
   --echo # The priorization system should treat all sessions belonging to the same
   --echo # thread_pool group and having the same properties (value of task_* and
   --echo # high_priority_connection) sufficient fair.
   --echo # This means IMHO the throughput (mysqltest.book_keeping.counter) of any
   --echo # of such sessions must be between 0.5 AVG(counter) and 2 AVG(counter)
   --echo # except the throughput is already a very small number (AVG(counter) < 10).
   --echo # That this criterion might become invalid in case of sessions fighting
   --echo # for getting the same exclusive locks.
   SELECT MIN(counter), AVG(counter) * 0.5, MAX(counter), AVG(counter) * 2
   FROM mysqltest.book_keeping
   GROUP BY high_priority_connection,task_size,task_transaction,task_prepared
   HAVING COUNT(*) > 1 AND AVG(counter) >= 10;
   SELECT *
   FROM mysqltest.book_keeping;
   --echo # abort
   exit;
}

if(!$DEBUG)
{
   let $num= $worker_count;
   while($num)
   {
      let $worker_stdout=   $MYSQLTEST_VARDIR/tmp/$num.out;
      let $worker_stderr=   $MYSQLTEST_VARDIR/tmp/$num.err;
      --error 0,1
      remove_file $worker_stdout 10;
      --error 0,1
      remove_file $worker_stderr 10;
      dec $num;
   }
}

#-------------------------------------------

SELECT AVG(tab1.counter),AVG(tab2.counter) INTO @first_counter,@second_counter
FROM mysqltest.book_keeping tab1, mysqltest.book_keeping tab2
WHERE tab1.worker_type = 'first' AND tab2.worker_type = 'second';
#
eval INSERT INTO mysqltest.statistics (prio_kickup_timer, first_counter, second_counter, $statistics_column_list)
SELECT @@GLOBAL.thread_pool_prio_kickup_timer, @first_counter, @second_counter,
tab2.QUERIES_EXECUTED       - tab1.QUERIES_EXECUTED,      tab2.QUERIES_QUEUED            - tab1.QUERIES_QUEUED,
tab2.PRIO_KICKUPS           - tab1.PRIO_KICKUPS,          tab2.STALLED_QUERIES_EXECUTED  - tab1.STALLED_QUERIES_EXECUTED,
tab2.BECOME_CONSUMER_THREAD - tab1.BECOME_CONSUMER_THREAD,tab2.BECOME_RESERVE_THREAD     - tab1.BECOME_RESERVE_THREAD,
tab2.BECOME_WAITING_THREAD  - tab1.BECOME_WAITING_THREAD, tab2.WAKE_THREAD_STALL_CHECKER - tab1.WAKE_THREAD_STALL_CHECKER,
tab2.SLEEP_WAITS            - tab1.SLEEP_WAITS,           tab2.DISK_IO_WAITS             - tab1.DISK_IO_WAITS,
tab2.ROW_LOCK_WAITS         - tab1.ROW_LOCK_WAITS,        tab2.GLOBAL_LOCK_WAITS         - tab1.GLOBAL_LOCK_WAITS,
tab2.META_DATA_LOCK_WAITS   - tab1.META_DATA_LOCK_WAITS,  tab2.TABLE_LOCK_WAITS          - tab1.TABLE_LOCK_WAITS,
tab2.USER_LOCK_WAITS        - tab1.USER_LOCK_WAITS,       tab2.BINLOG_WAITS              - tab1.BINLOG_WAITS,
tab2.GROUP_COMMIT_WAITS     - tab1.GROUP_COMMIT_WAITS,    tab2.FSYNC_WAITS               - tab1.FSYNC_WAITS
FROM mysqltest.start_statistics tab1, mysqltest.start_statistics tab2
WHERE tab1.WHAT = 'before' AND tab2.WHAT = 'after';

--disable_query_log
--disable_result_log
if($DEBUG)
{
   --enable_query_log
   --enable_result_log
   SELECT
   'MY_SETTINGS',@@GLOBAL.thread_pool_prio_kickup_timer,
   @first_high_priority_connection,  @first_size,  @first_transaction,  @first_prepared,
   @second_high_priority_connection, @second_size, @second_transaction, @second_prepared;
   SELECT 'MY_RESULTS', @@GLOBAL.thread_pool_prio_kickup_timer, @first_counter, @second_counter,
   tab2.QUERIES_EXECUTED       - tab1.QUERIES_EXECUTED,      tab2.QUERIES_QUEUED            - tab1.QUERIES_QUEUED,
   tab2.PRIO_KICKUPS           - tab1.PRIO_KICKUPS,          tab2.STALLED_QUERIES_EXECUTED  - tab1.STALLED_QUERIES_EXECUTED,
   tab2.BECOME_CONSUMER_THREAD - tab1.BECOME_CONSUMER_THREAD,tab2.BECOME_RESERVE_THREAD     - tab1.BECOME_RESERVE_THREAD,
   tab2.BECOME_WAITING_THREAD  - tab1.BECOME_WAITING_THREAD, tab2.WAKE_THREAD_STALL_CHECKER - tab1.WAKE_THREAD_STALL_CHECKER,
   tab2.SLEEP_WAITS            - tab1.SLEEP_WAITS,           tab2.DISK_IO_WAITS             - tab1.DISK_IO_WAITS,
   tab2.ROW_LOCK_WAITS         - tab1.ROW_LOCK_WAITS,        tab2.GLOBAL_LOCK_WAITS         - tab1.GLOBAL_LOCK_WAITS,
   tab2.META_DATA_LOCK_WAITS   - tab1.META_DATA_LOCK_WAITS,  tab2.TABLE_LOCK_WAITS          - tab1.TABLE_LOCK_WAITS,
   tab2.USER_LOCK_WAITS        - tab1.USER_LOCK_WAITS,       tab2.BINLOG_WAITS              - tab1.BINLOG_WAITS,
   tab2.GROUP_COMMIT_WAITS     - tab1.GROUP_COMMIT_WAITS,    tab2.FSYNC_WAITS               - tab1.FSYNC_WAITS
   FROM mysqltest.start_statistics tab1, mysqltest.start_statistics tab2
   WHERE tab1.WHAT = 'before' AND tab2.WHAT = 'after';
}


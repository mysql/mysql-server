#
# The macro processes a DDL statement while its handling is
# interrupted by simulated crashes at time of the query
# has not yet been logged and upon that.
# The first, pre-binlog crash happens right after prepare in SEs
# (storage engines), before binary log event for the statement is
# flushed to the disk.  In this case statement should be correctly
# rolled back by recovery.  The second, post-binlog crash to the
# repeated statement occurs after binary log event is flushed to the
# disk, before commit in the SEs.
# The query results must be found committed upon the server restart.
# The binlog is rotated at the beginning of either crash simulation
# and its resulted file is memorized to optionally print out
# its events which must contain a new XID field.
#
# Another purpose of the macro to run simply as a load
# generator e.g for a slave server without crash simulations
# ($do_only_regular_logging).
#
# The macro increments
#       $count_ddl_queries
# per its invocation which serves as a counter for the number of
# processed queries.
#
# Usage, e.g:
#
# --let $do_pre_binlog=1
# --let $do_post_binlog=1
# --let $do_only_regular_logging=1 # run only query no crash simulation
# --let $ddl_query=CREATE USER user1
# --let $pre_binlog_crash_check= [SELECT count(*) = 0 FROM mysql.user WHERE user = 'user1']
# --let $post_binlog_crash_check= [SELECT count(*) = 1 FROM mysql.user WHERE user = 'user1']
#
# --let $do_show_binlog_events=1 # invoke show-binlog-events at the
#                                # end of query execution
# --let $do_count_queries  # When set the processed queries are counted in $count_ddl_queries
# --let $master_log_table  # DDL query and post-recovery checks can be logged in a table
# --let $master_log_db     # The database of the log table
#
# here a value in [] can be just empty to indicate no check after
# the corresponding crash is done.
#

if ($do_count_queries)
{
  --inc $count_ddl_queries
}

--disable_query_log
if ($master_log_table)
{
  --let $save_curr_db=`SELECT database()`
  if ($master_log_db)
  {
    --eval USE $master_log_db
  }
  --eval INSERT INTO $master_log_table SET id= NULL, ddl_query= "$ddl_query", pre_binlog_check= "$pre_binlog_crash_check", post_binlog_check= "$post_binlog_crash_check"
 --eval USE $save_curr_db
}
--enable_query_log

# $do_only_regular_logging is incompatible with crash simulating $do:s.
if ($do_only_regular_logging)
{
  if ($do_pre_binlog)
  {
    --echo *** Wrong option combination! ***
    --die
  }
  if ($do_post_binlog)
  {
    --echo *** Wrong option combination! ***
    --die
  }
}

--source include/gtid_step_reset.inc

if ($do_pre_binlog)
{
  FLUSH LOGS;
  --let $binlog_file= query_get_value("SHOW MASTER STATUS", File, 1)

  #
  # prepare, the first CRASH,  /* log,  commit */
  #
  --let $gen_ddl_query = `SELECT REPLACE("$ddl_query",'.so','_LIB')`
  --let $gen_ddl_query = `SELECT REPLACE("$gen_ddl_query",'.dll','_LIB')`
  --echo *** Crash right after '$gen_ddl_query'  has been prepared in the engine before being logged ***
  --source include/expect_crash.inc

  SET @@SESSION.debug="+d,crash_commit_before_log";
  --disable_query_log
  if ($manual_gtid_next)
  {
    --eval $manual_gtid_next
  }
  --enable_query_log
  --replace_result .so _LIB .dll _LIB
  --error 2013
  --eval $ddl_query

  #
  # restart the server
  #
  --source include/start_mysqld.inc

  if ($pre_binlog_crash_check)
  {
    if (!`$pre_binlog_crash_check`)
    {
      --echo *** State check upon recovery after pre-binlog crash fails ***
      --die
    }
  }

  if ($do_show_binlog_events)
  {
    --let $keep_ddl_xid=1
    --let $show_binlog_events_mask_columns=1,2,4,5
    --source include/show_binlog_events.inc
  }

  # The Gtid check, when its mode ON.
  # After rollback-recovery of this branch Gtid executed must
  # remain as was before the query execution.
  --let $gtid_step_count=0
  --source include/gtid_step_assert.inc

} # end of $do_pre_binlog

if ($do_post_binlog)
{
  FLUSH LOGS;
  --let $binlog_file= query_get_value("SHOW MASTER STATUS", File, 1)

  #
  # prepare, log, the 2nd CRASH, /* commit */
  #
  --let $gen_ddl_query = `SELECT REPLACE("$ddl_query",'.so','_LIB')`
  --let $gen_ddl_query = `SELECT REPLACE("$gen_ddl_query",'.dll','_LIB')`
  --echo *** Crash right after '$gen_ddl_query' has been binary-logged before committed in the engine ***
  --source include/expect_crash.inc

  SET @@SESSION.debug="+d,crash_commit_after_log";
  # End of Debug
  --disable_query_log
  if ($manual_gtid_next)
  {
    --eval $manual_gtid_next
  }
  --enable_query_log
  --replace_result .so _LIB .dll _LIB
  --error 2013
  --eval $ddl_query

  #
  # restart the server
  #
  --source include/start_mysqld.inc

  if ($post_binlog_crash_check)
  {
    if (!`$post_binlog_crash_check`)
    {
      --echo *** State check upon recovery after pre-commit crash fails ***
      --die
    }
  }
  if ($do_show_binlog_events)
  {
    --let $keep_ddl_xid=1
    --let $show_binlog_events_mask_columns=1,2,4,5
    --source include/show_binlog_events.inc
  }

  # The Gtid check, when its mode ON.
  # After commit-recovery of this branch Gtid executed must
  # receive a new gtid item of the current DDL transaction.

  --let $gtid_step_count=1
  --source include/gtid_step_assert.inc

} # end of $do_post_binlog

if ($do_only_regular_logging)
{
  FLUSH LOGS;
  # Some uses of this involve the name of a library, mask OS difference
  --replace_result .so _LIB .dll _LIB
  --eval $ddl_query
  if ($do_show_binlog_events)
  {
    --let $keep_ddl_xid=1
    --let $show_binlog_events_mask_columns=1,2,4,5
    --source include/show_binlog_events.inc
  }

  # The Gtid check, when its mode ON.
  # After commit-recovery of this branch Gtid executed must
  # receive a new gtid item of the current DDL transaction.

  --let $gtid_step_count=1
  --source include/gtid_step_assert.inc
}

#
# Cleanup
#
--let $keep_ddl_xid=
--let $show_binlog_events_mask_columns=

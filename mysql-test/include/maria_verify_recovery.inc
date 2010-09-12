# Maria helper script.
# Runs recovery, compare with expected table data.

# API:
# 1) set $mms_tname to a string, and $mms_tables to a number N, the script
# will cover tables mysqltest.$mms_tname1,...$mms_tnameN
# 2) set $mvr_debug_option to the crash way
# 3) set $mvr_crash_statement to the statement which will trigger a crash
# 4) set $mvr_restore_old_snapshot to 1 if you want recovery to run on
# an old copy of tables and of the control file, 0 for normal recovery.
# 5) set $mms_compare_physically to 1 if you want a physical byte-for-byte
# comparison with expected table. Checksum comparison is always done.
# "mvr" is a namespace for Maria_Verify_Recovery

connection admin;

# we may do a copy-back  of tables before comparison, so save comparison
# request made by caller:
let $mms_compare_physically_save=$mms_compare_physically;
let $mms_compare_physically=0;

# warn mtr that mysqld is going to die and should not be restarted immediately
#append_file $MYSQLTEST_VARDIR/tmp/mysqld.1.expect;
#wait-maria_verify_recovery.inc
#EOF
# todo: remove this "system" and uncomment above when BUG#32296 is fixed
system echo wait-maria_verify_recovery.inc >> $MYSQLTEST_VARDIR/tmp/mysqld.1.expect;

# flush page cache and log, only log, or nothing, and kill mysqld with
# abort().
# When we restore an old snapshot, we could just kill mysqld nicely,
# but that would implicitely commit all work, which the tester may
# not want (tester may want to observe rollback happening).

eval SET SESSION debug=$mvr_debug_option;
--echo * crashing mysqld intentionally
--error 2013
eval $mvr_crash_statement; # this will crash (DBUG magic)

if ($mvr_restore_old_snapshot)
{

  # copy snapshot made by maria_make_snapshot_for_feeding_recovery back
  # into datadir.

  let $mms_table_to_use=$mms_tables;
  let $mms_purpose=feeding_recovery;
  let $mms_reverse_copy=1;
  while ($mms_table_to_use)
  {
    -- source include/maria_make_snapshot.inc
    dec $mms_table_to_use;
  }
  let $mms_reverse_copy=0;

  # also copy back control file, to force recovery to start from an early
  # point, ignoring further checkpoints.
  -- error 0,1
  remove_file $MYSQLD_DATADIR/$MARIA_LOG/aria_log_control;
  copy_file $MYSQLTEST_VARDIR/tmp/mms_for_$mms_purpose.aria_log_control $MYSQLD_DATADIR/$MARIA_LOG/aria_log_control;
}

--echo * recovery happens
# let mtr restart mysqld (and thus execute the maria log)
#append_file $MYSQLTEST_VARDIR/tmp/mysqld.1.expect;
#restart-maria_verify_recovery.inc
#EOF
system echo restart-maria_verify_recovery.inc >> $MYSQLTEST_VARDIR/tmp/mysqld.1.expect;

--source include/wait_until_connected_again.inc

# Compare that tables of $mms_tables are identical to old.
# We always compare with CHECKSUM TABLE, and if requested (which makes sense
# only for testing the REDO phase, as UNDO phase generates new records so new
# LSNs on pages.) with a physical byte-for-byte comparison.
let $mms_table_to_use=$mms_tables;
let $mms_purpose=comparison;
let $mms_compare_physically=$mms_compare_physically_save;
while ($mms_table_to_use)
{
  eval check table $mms_tname$mms_table_to_use extended;
  --echo * testing that checksum after recovery is as expected
  let $new_checksum=`CHECKSUM TABLE $mms_tname$mms_table_to_use`;
  let $old_checksum=`CHECKSUM TABLE mysqltest_for_$mms_purpose.$mms_tname$mms_table_to_use`;
  # the $ text variables above are of the form "db.tablename\tchecksum",
  # as db differs, we use substring().
  --disable_query_log
  eval select if(substring("$new_checksum",instr("$new_checksum",".t1")) = substring("$old_checksum",instr("$old_checksum",".t1")),"ok","failure") as "Checksum-check";
  --enable_query_log
  # this script may compare physically or do nothing
  -- source include/maria_make_snapshot.inc
  dec $mms_table_to_use;
}

connection default;
# the effect of "use" is lost after a restart so we are back into db "test"
use mysqltest;

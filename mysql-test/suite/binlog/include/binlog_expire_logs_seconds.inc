#
# WL#9237: Add a new variable binlog_expire_logs_seconds

# Here we will test purging of binary logs when either one or both of these variables are set
# - binlog_expire_logs_seconds
# - expire_logs_days

# The three scenarios being tested for are:
# 1. FLUSH LOGS
# 2. Rotation of logs because of binlog growing bigger than max_binlog_size
# 3. Server restart
#
# Usuage: --let $binlog_expire_logs_seconds=
#         --let $expire_logs_days=
#
# --source suite/binlog/include/binlog_expire_logs_seconds.inc

--let $expire_logs_seconds= `SELECT @@global.binlog_expire_logs_seconds + @@global.expire_logs_days * 24 * 60 * 60`

CREATE TABLE t1(s LONGBLOB );

--let $max_binlog_size_save= `SELECT @@GLOBAL.MAX_BINLOG_SIZE`

--let $case= 0

while ($case < 3)
{
  --echo Case:$case
  --let $first_binlog_file= query_get_value(SHOW MASTER STATUS, File, 1)

  # rotates the log, thence the first log will be closed, and depending upon
  # the expire time the purge will/will not happen.

  FLUSH LOGS;

  INSERT INTO t1 VALUES('a');

  --let $second_binlog_file= query_get_value(SHOW MASTER STATUS, File, 1)

  FLUSH LOGS;


  # This is done to avoid time out in cases where the expire time is more.
  # What we do is in those cases modify the timestamp of the oldest log file
  # to be the same as expire time so when we execute the next flush log command
  # the oldest log will be purged.

  # Only change the timestamp of binlog file when the expire_logs_seconds which is the total
  # time for expiring log is greater than 30 seconds

  if ($expire_logs_seconds > 30)
  {
    --let _EXPIRE_TIME= `SELECT $expire_logs_seconds + 60`
    --let _FIRST_BINLOG_FILE= $MYSQLD_DATADIR/$first_binlog_file
--perl
    use strict;
    use warnings;
    my $expire_time = $ENV{'_EXPIRE_TIME'};
    my $first_binlog_file = $ENV{'_FIRST_BINLOG_FILE'};
    my $epoch = (stat($first_binlog_file))[9];
    my $mtime = $epoch - $expire_time;
    utime $mtime, $mtime, $first_binlog_file;
  EOF
  }

  # Checking this ensures that nothing is purged so far.
  --file_exists $MYSQLD_DATADIR/$first_binlog_file

  if ($case == 0)
  {
    --echo #### 1. FLUSH LOGS

    FLUSH LOGS;
  }
  if ($case == 1)
  {
    --echo #### 2. Binlog_size > max_binlog_size

    SET @@GLOBAL.MAX_BINLOG_SIZE= 4096;

    INSERT INTO t1 (s) VALUES (REPEAT('s',50000));
  }
  if ($case == 2)
  {
    --echo #### 3. Server restart

    --let $restart_parameters=restart:--binlog_expire_logs_seconds=$expire_logs_seconds
    --source include/restart_mysqld.inc
  }

  if ($expire_logs_seconds != 0)
  {
    --error 1
    --file_exists $MYSQLD_DATADIR/$first_binlog_file
  }
  if ($expire_logs_seconds == 0)
  {
    --file_exists $MYSQLD_DATADIR/$first_binlog_file
  }
  --file_exists $MYSQLD_DATADIR/$second_binlog_file

  --inc $case
  RESET MASTER;
}
--echo ##### Cleanup #####
--eval SET @@GLOBAL.MAX_BINLOG_SIZE= $max_binlog_size_save;
DROP TABLE t1;
RESET MASTER;


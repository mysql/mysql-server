# Wait for node $_ndb_nodeid
# to be in start phase $_start_phase
#
# Extend in future to allow 'or greater'
--echo Waiting for node $_ndb_nodeid to enter start phase $_start_phase

--let $success = 0
--let $tmpfile = $MYSQLTEST_VARDIR/tmp/wait_start_phase.txt
--let $maxtime = 120

--disable_query_log
--disable_result_log

# Use temporary table to avoid problems with needing GSL
# to create non-temporary table, and GSL being unavailable
# as cluster is restarting
#
create temporary table test.tmpfile (txt varchar(200));

while (! $success)
{
  --exec $NDB_MGM -e "$_ndb_nodeid STATUS" > $tmpfile
  eval LOAD DATA INFILE '$tmpfile' INTO TABLE test.tmpfile;
  # select * from test.tmpfile;
  eval select count(1) from test.tmpfile where txt like "%starting (Last completed phase $_start_phase)%" into @match_count;

  --let $success = query_get_value(select @match_count as m, m, 1)
  delete from test.tmpfile;
  --sleep 1
  --dec $maxtime
  if (!$maxtime)
  {
    --echo "Timeout waiting for $_ndb_nodeid to enter start phase $_start_phase"
    die "Timeout waiting for startphase";
  }
}

drop table test.tmpfile;
--remove_file $tmpfile

--enable_query_log
--enable_result_log


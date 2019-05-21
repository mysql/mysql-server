#
# Wait until the MySQL Server has connected properly to the cluster
# and come out of readonly mode
#

--disable_query_log
--disable_result_log

# Turn off binary logging for this session
set @saved_log = @@sql_log_bin;
set sql_log_bin = 0;

let $counter = 600;
let $connected = 0;
while (!$connected)
{

  # Disable "abort on error" and run some queries to check if cluster
  # is operational
  --disable_abort_on_error
  CREATE TABLE check_not_readonly (a int primary key) engine = NDB;
  INSERT INTO check_not_readonly VALUES(1);
  --enable_abort_on_error

  if (!$mysql_errno)
  {
    let $connected = 1;
  }
  # else
  if ($mysql_errno)
  {
    if (!$counter)
    {
      die Timeout while waiting for MySQL Server to connect to cluster;
    }
    dec $counter;
    --sleep 0.1
  }
}

# Drop the table.
# Allow retry when failure to lock GSL occurs, this is due
# to contention when starting more than one MySQL Server who
# need to lock the GSL to sync DD.

let $drop_counter = 600;
let $dropped = 0;
while(!$dropped)
{
  --error 0, ER_LOCK_WAIT_TIMEOUT
  DROP TABLE check_not_readonly;

  if (!$mysql_errno)
  {
    let $dropped = 1;
  }
  # else
  if ($mysql_errno)
  {
    dec $drop_counter;
    if (!$drop_counter)
    {
      die Timeout while waiting for 'DROP TABLE check_not_readonly' to succeed;
    }
    --sleep 0.1
  }
}

# Restore binary logging for this session
set sql_log_bin = @saved_log;

--enable_query_log
--enable_result_log

#
# connected
#

#
# Check if event exists for table
#
# Arguments:
#  check_event_tabname - name of table
#  check_event_dbname - db of table
#  [check_event_full - expect a full event]
#
#

if (!$check_event_tabname)
{
  die Need the name of table to check event for;
}

if (!$check_event_dbname)
{
  die Need the db of table to check event for;
}

#
# The name for an event has form REPL[F]$<db>/<table_name>
# - the additional F is used when table should be binlogged
#   with "full" rows
#
let $event_prefix = REPL;
if ($check_event_full)
{
  let $event_prefix = REPLF;
}
# Expected event name, includes extra escape for the dollar
# sign avoiding that mysqltest evaluates it before sending the query
let $expected_event_name =
  $event_prefix\\\$$check_event_dbname/$check_event_tabname;
#echo expected_event_name: $expected_event_name;
let $found_event = `SELECT count(*) FROM ndbinfo.events
                      WHERE name = '$expected_event_name'`;
if ($found_event)
{
  echo Found event for '$check_event_dbname.$check_event_tabname';
}
if (!$found_event)
{
  echo No event found for '$check_event_dbname.$check_event_tabname';
}

# Reset args
let $check_event_tabname = ;
let $check_event_dbname = ;
let $check_event_full = ;

--enable_query_log

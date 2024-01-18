# For a given event which is at position $source_log_pos in the the master's
# binary log, returns its position in the slave's relay log file
# $relay_log_file.  
# The position is stored in the variable $relay_log_pos.

# Usage:
#   let $relay_log_file= 'relay-log-bin.000001'; 
#   let $source_log_pos= 106; 
#   source include/rpl/convert_binlog_position_to_relay_log_position.inc; 
#   # at this point, rpl/convert_binlog_position_to_relay_log_position.inc sets $relay_log_pos. echo position
#   # in $relay_log_file: $relay_log_pos. 

if (!$relay_log_file)
{
  --die 'variable $relay_log_file is null'
}

if (!$source_log_pos)
{
  --die 'variable $source_log_pos is null'
}

let $MYSQLD_DATADIR= `select @@datadir`;
let $_suffix= `SELECT UUID()`;
let $_tmp_file= $MYSQLTEST_VARDIR/tmp/mysqlbinlog.$_suffix;
--exec $MYSQL_BINLOG $MYSQLD_DATADIR/$relay_log_file > $_tmp_file

# All queries in this file should not be logged.
--disable_query_log

--disable_warnings
DROP TEMPORARY TABLE IF EXISTS mysqlbinlog_events;
DROP TEMPORARY TABLE IF EXISTS events_at;
DROP TEMPORARY TABLE IF EXISTS events_pos;
CREATE TEMPORARY TABLE mysqlbinlog_events(c1 INT AUTO_INCREMENT KEY, c2 varchar(256));

# Event position is in the comments output by mysqlbinlog, we load this
# comments into the table 
# '# at 106' 
# '# ....  end_log_pos 46'
eval LOAD DATA LOCAL INFILE '$_tmp_file' INTO TABLE mysqlbinlog_events
  LINES STARTING BY '#' (c2) SET c1 = NULL;

# Event pos in relay log file is inserted into table events_at
CREATE TEMPORARY TABLE events_at(c1 INT AUTO_INCREMENT KEY, c2 varchar(256))
  SELECT c2 FROM mysqlbinlog_events WHERE c2 LIKE ' at%' ORDER BY c1;

# Event pos in source log file is inserted into table events_pos
CREATE TEMPORARY TABLE events_pos(c1 INT AUTO_INCREMENT KEY, c2 varchar(256))
  SELECT c2 FROM mysqlbinlog_events WHERE c2 LIKE '% end_log_pos %' ORDER BY c1;
--enable_warnings

#  events_at                                events_pos
#  c1------c2--------------------------     c1------c2------------------------
#  1       ev1's begin pos in relay log     1      ev1's end pos in master log
#  2       ev2's begin pos in relay log     2      ev2's end pos in master log
#  3       ev3's begin pos in relay log     3      ev3's end pos in master log
#  events always keep the same sequence.  
#  Because event[N]'s end pos is equal to event[N+1]'s begin pos we want to
#  find event's end pos in relay log, we can find the right relay_log_pos
#  using the relationship that 'events_pos.c1 = events_at.c1 + 1'
# 
# There is a fault that we can't get the relay log position of the last event,
# as it is not output by mysqlbinlog
let $relay_log_pos= `SELECT SUBSTRING(a.c2, 5)
  FROM events_at a, events_pos b
  WHERE a.c1=b.c1+1 and b.c2 LIKE '% $source_log_pos%'`;
DROP TEMPORARY TABLE mysqlbinlog_events, events_at, events_pos;
--remove_file $_tmp_file
--enable_query_log

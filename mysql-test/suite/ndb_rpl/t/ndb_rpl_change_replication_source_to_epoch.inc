# Change replication source coordinates to those for the given epoch ($the_epoch)
#
# Uses the Position and File of the *next* entry after
# the entry for the given epoch.
# When restarting from a real epoch we can start using
# the next_file and next_position of the actual epoch
# row, but when starting from a 'backup' epoch
# (GCI << 32 | 0xffffffff), we must find either the
# preceding or following epoch
#
#

--disable_query_log
--disable_result_log
--connection master
#--eval select * from mysql.ndb_binlog_index where epoch > $first_backup_epoch LIMIT 1;

let $exact_count=0;
let $after_count=0;
let $before_count=0;

set @exact_count = 0;
eval select @exact_count:=count(1)
       from mysql.ndb_binlog_index where epoch = $the_epoch LIMIT 1;
let $exact_count = query_get_value(select @exact_count as ec, ec, 1);

set @after_count = 0;
eval select @after_count:=count(1)
       from mysql.ndb_binlog_index where epoch > $the_epoch
       order by epoch asc LIMIT 1;
let $after_count = query_get_value(select @after_count as ac, ac, 1);

set @before_count = 0;
eval select @before_count:=count(1)
       from mysql.ndb_binlog_index where epoch < $the_epoch
       order by epoch desc LIMIT 1;
let $before_count = query_get_value(select @before_count as bc, bc, 1);


if ($exact_count)
{
  # Have an entry corresponding exactly to the restore-point epoch
  # So start from *next* location in the Binlog described in that entry
  eval select @the_pos:=next_position,
    @the_file:=SUBSTRING_INDEX(REPLACE(next_file,'\\\\','/'), '/', -1)
    from mysql.ndb_binlog_index where epoch = $the_epoch LIMIT 1;

  #--echo Choosing exact position for $the_epoch
}
if (!$exact_count)
{
  if ($after_count)
  {
    # Have an entry after the restore point epoch
    # Assume that nothing is missing between restore point and this first
    # following epoch
    # Start from beginning of this following epoch
    eval select @the_pos:=position,
            @the_file:=SUBSTRING_INDEX(REPLACE(file,'\\\\','/'), '/', -1)
            from mysql.ndb_binlog_index where epoch > $the_epoch
            order by epoch asc LIMIT 1;
    #--echo Choosing first position after restore point for $the_epoch
  }
  if (!$after_count)
  {
    if ($before_count)
    {
      # Have no entry with the exact epoch, or following it
      # However we can get a binlog start point by starting from just after
      # the preceding epoch in the binlog, assuming nothing is missing.
      #
      eval select @the_pos:=next_position,
            @the_file:=SUBSTRING_INDEX(REPLACE(next_file,'\\\\','/'), '/', -1)
            from mysql.ndb_binlog_index where epoch < $the_epoch
            order by epoch desc LIMIT 1;
      #--echo Choosing last position before restore point for $the_epoch
    }
    if (!$before_count)
    {
      # Have no position at all binlog index table is empty
      # Could be optional to allow this and just start from first binlog etc
      # But let's leave this
      #
      --echo No position info available in masters ndb_binlog_index table
      --echo Epoch : $the_epoch
      select * from mysql.ndb_binlog_index;
      --die Cannot continue
    }
  }
}

let $the_pos=0;
let $the_file="BADFILE";
let $the_pos  = query_get_value(select @the_pos as pos, pos, 1);
let $the_file = query_get_value(select @the_file as file, file, 1);

--connection slave

RESET REPLICA;
--eval CHANGE REPLICATION SOURCE to SOURCE_LOG_FILE="$the_file", SOURCE_LOG_POS=$the_pos;

--enable_result_log
--enable_query_log

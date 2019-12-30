# Change master coordinates to those for the given epoch ($the_epoch)
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

eval select @the_pos:=position,
            @the_file:=SUBSTRING_INDEX(REPLACE(file,'\\\\','/'), '/', -1)
            from mysql.ndb_binlog_index where epoch > $the_epoch LIMIT 1;
let $the_pos  = query_get_value(select @the_pos as pos, pos, 1);
let $the_file = query_get_value(select @the_file as file, file, 1);

--connection slave

reset slave;
--eval change master to master_log_file="$the_file", master_log_pos=$the_pos;

--enable_result_log
--enable_query_log

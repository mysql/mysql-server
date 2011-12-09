##################################################
# Author: Jeb
# Date:   2007/04
# Purpose: To select out log name, start and end
#          positions from ndb_apply_status table
##################################################
--replace_column 1 <log_name> 2 <start_pos> 3 <end_pos>
select @log_name:=log_name, @start_pos:=start_pos, @end_pos:=end_pos
       from mysql.ndb_apply_status;
--let $start_pos = `select @start_pos`
--let $end_pos = `select @end_pos`
--let $log_name = `select @log_name`


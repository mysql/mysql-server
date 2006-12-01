#
# now setup replication to continue from last epoch
# 1. get ndb_apply_status epoch from slave
# 2. get corresponding _next_ binlog postition from master
# 3. change master on slave

# 1.
--connection slave
--replace_column 1 <the_epoch>
SELECT @the_epoch:=MAX(epoch) FROM mysql.ndb_apply_status;
--let $the_epoch= `select @the_epoch`

# 2.
--connection master
--replace_result $the_epoch <the_epoch>
--replace_column 1 <the_pos>
eval SELECT @the_pos:=Position,@the_file:=SUBSTRING_INDEX(FILE, '/', -1)
   FROM mysql.ndb_binlog_index WHERE epoch > $the_epoch ORDER BY epoch ASC LIMIT 1;
--let $the_pos= `SELECT @the_pos`
--let $the_file= `SELECT @the_file`

# 3.
--connection slave
--replace_result $the_pos <the_pos>
eval CHANGE MASTER TO
  master_log_file = '$the_file',
  master_log_pos = $the_pos ;

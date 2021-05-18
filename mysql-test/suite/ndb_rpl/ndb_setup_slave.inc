#
# now setup replication to continue from last epoch
# 1. get ndb_apply_status epoch from slave
# 2. get corresponding _next_ binlog postition from master
# 3. change master on slave

# 1.

--connection slave
SELECT MAX(epoch) FROM mysql.ndb_apply_status into @the_epoch;
--replace_column 1 <the_epoch>
SELECT @the_epoch;
--let $the_epoch= `select @the_epoch`

# 2.
--connection master
--disable_query_log
eval SELECT Position, SUBSTRING_INDEX(REPLACE(FILE,'\\\\','/'), '/', -1)
   FROM mysql.ndb_binlog_index WHERE epoch > $the_epoch ORDER BY epoch ASC LIMIT 1 into @the_pos, @the_file;
--replace_result $the_epoch <the_epoch>
--replace_column 1 <the_pos>
eval SELECT @the_pos, @the_file;
--enable_query_log
--let $the_pos= `SELECT @the_pos`
--let $the_file= `SELECT @the_file`

# 3.
--connection slave
--replace_result $the_pos <the_pos>
eval CHANGE REPLICATION SOURCE TO
  SOURCE_LOG_FILE = '$the_file',
  SOURCE_LOG_POS = $the_pos ;

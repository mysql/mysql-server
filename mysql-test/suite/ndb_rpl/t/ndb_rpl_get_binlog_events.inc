# 
# Munch the mysqlbinlog tool output into deterministic form to look at server ids
#

--disable_query_log
let $_dump_file= $MYSQLTEST_VARDIR/tmp/rpl_ndb_mysqlbinlog.sql;
let $_datadir= `select @@datadir`;
--exec $MYSQL_BINLOG --verbose $_datadir/$BINLOG_FILENAME.000001 > $_dump_file
set sql_log_bin=0;
create table raw_binlog_rows (txt varchar(1000));
eval load data infile '$_dump_file' into table raw_binlog_rows
  columns terminated by '\n';
remove_file $_dump_file;

#
# Show the values for 'server id' and 'event type' in the mysqlbinlog output.
# Remove other non-deterministic and uninteresting elements.
#
echo select 'server id' and 'event type' from mysqlbinlog output;
replace_regex
  /#[0-9]*[ ]*[0-9]*:[0-9]*:[0-9]* server id /server id /
  /thread_id=[0-9]*//
  /exec_time=[0-9]*//
  /end_log_pos [0-9]*//
  /table id [0-9]*//
  /error_code=[0-9]*//
  /mapped to number [0-9]*//
  /CRC32 0x[0-9a-f]{8}//
  /last_committed=[0-9]*//
  /sequence_number=[0-9]*//
  /rbr_only=[a-z]+//
  /Xid = [0-9]*//
  /original_committed_timestamp=[0-9]*.immediate_commit_timestamp=[0-9]*//
  /transaction_length=[0-9]*//
  /\t//
  ;
select replace(txt, '\r', '') as binlog_entries from raw_binlog_rows
    where txt like '%server id%'
      and not (txt like '%Start%' or
               txt like '%Rotate%' or
               txt like '%Anonymous\_GTID%' or
               txt like '%Previous-GTIDs%');

drop table raw_binlog_rows;
set sql_log_bin=1;
--enable_query_log

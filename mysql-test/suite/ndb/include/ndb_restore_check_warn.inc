--source include/have_ndb.inc

--let $dump_file= $MYSQLTEST_VARDIR/tmp/restore_out.txt


--disable_query_log
create table test.ndb_restore_output(row varchar(1000))engine=ndb;
eval load data infile '$dump_file' into table test.ndb_restore_output;

# remove <CR> from row in Windows
update test.ndb_restore_output set row = trim(trailing '\r' from row);
select * from test.ndb_restore_output where row like "%WARNING%" order by row;

drop table test.ndb_restore_output;
--enable_query_log

# verify that ndb_select_all unpacks the fields correctly and
# displays them like mysql, with 2 differences accounted for:
#
# date/time uses "/" as separator instead of blank
# timestamp is displayed as unix timestamp

--let $dump_file = $MYSQLTEST_VARDIR/tmp/wl946_select_all.txt
--exec $NDB_SELECT_ALL --no-defaults -d test --delimiter=, t1 | grep '^[0-9]*,' > $dump_file

create temporary table t1tmp (
  a int primary key,
  b char(40),
  c char(40),
  d char(40),
  e char(40),
  f char(40)
) engine=myisam;

--chmod 0777 $dump_file
--replace_result $dump_file DUMP_FILE
eval load data infile '$dump_file' into table t1tmp
fields terminated by ',';
--remove_file $dump_file

select count(*), sum(crc32(concat(a,b,c,d,e,unix_timestamp(f)))) from t1;
select count(*), sum(crc32(concat(a,b,c,d,replace(e,'/',' '),f))) from t1tmp;

drop table t1tmp;

# verify that ndb_select_all unpacks the fields correctly and
# displays them like mysql, with 2 differences accounted for:
#
# date/time uses "/" as separator instead of blank
# timestamp is displayed as unix timestamp
#
# use mysql to grep out non-data lines

--let $dump_file1 = $MYSQLTEST_VARDIR/tmp/wl946_select_all1.txt
--let $dump_file2 = $MYSQLTEST_VARDIR/tmp/wl946_select_all2.txt

--exec $NDB_SELECT_ALL --no-defaults -d test --delimiter=, t1 > $dump_file1

create temporary table t1tmp1 (
  x char(255)
) engine=myisam;

--chmod 0777 $dump_file1
--replace_result $dump_file1 DUMP_FILE1
eval load data infile '$dump_file1' into table t1tmp1;

delete from t1tmp1 where x not regexp '^[0-9]*,';
--replace_result $dump_file2 DUMP_FILE2
eval select x from t1tmp1 into outfile '$dump_file2';

create temporary table t1tmp2 (
  a int primary key,
  b char(40),
  c char(40),
  d char(40),
  e char(40),
  f char(40)
) engine=myisam;

--chmod 0777 $dump_file2
--replace_result $dump_file2 DUMP_FILE2
eval load data infile '$dump_file2' into table t1tmp2
fields terminated by ',' lines terminated by '\n';

# remove any windows CR from last field
update t1tmp2 set f = replace(f, '\r', '');

select count(*), sum(crc32(concat(a,b,c,d,e,unix_timestamp(f)))) from t1;
select count(*), sum(crc32(concat(a,b,c,d,replace(e,'/',' '),f))) from t1tmp2;

drop table t1tmp1;
drop table t1tmp2;

--remove_file $dump_file1
--remove_file $dump_file2

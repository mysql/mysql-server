# Check for IBM i 6.1 or later
--disable_query_log
system uname -rv > $MYSQLTEST_VARDIR/tmp/version;
--disable_warnings
drop table if exists uname_vr;
--enable_warnings
create temporary table uname_vr (r int, v int);
--disable_warnings
eval LOAD DATA INFILE "$MYSQLTEST_VARDIR/tmp/version" into table uname_vr fields terminated by ' ';
--enable_warnings
let $ok = `select count(*) from uname_vr where v > 5`;
drop table uname_vr;
remove_file $MYSQLTEST_VARDIR/tmp/version;
--enable_query_log
if (!$ok)
{
  skip "Need IBM i 6.1 or later";
}
  


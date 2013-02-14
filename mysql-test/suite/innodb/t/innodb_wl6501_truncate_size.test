# =====================  purpose  ========================================
#
# The test verifies that 'TRUNCATE TABLE' statement works fine and the
# size of .ibd file is equal to the truncated size after truncation.
#

--disable_warnings
drop table if exists t1;
--enable_warnings

create table t1(a int not null,
 b int not null,
 primary key (a),
 index (b))
engine=innodb;

-- disable_query_log
-- disable_result_log
let $n=20000;
while ($n)
{
  eval insert into t1 values($n,$n);
  dec $n;
}
-- enable_result_log
-- enable_query_log
select count(*) from t1;

truncate table t1;
-- echo # Verify that 'TRUNCATE TABLE' statement works fine, which can empty a table.
select count(*) from t1;

-- echo # Check the size of .ibd file after truncation.
perl;
open F, ">>".$ENV{'MYSQLTEST_VARDIR'}.'/tmp/ibdfile.size' or die "Failed to open t1.ibd file";
my $path = $ENV{'MYSQLTEST_VARDIR'}.'/mysqld.1/data/test/t1.ibd';
my $filesize = -s $path;
printf F ("The size of t1.ibd file is %d bytes.\n", $filesize);
close F;
EOF
cat_file $MYSQLTEST_VARDIR/tmp/ibdfile.size;
remove_file $MYSQLTEST_VARDIR/tmp/ibdfile.size;

-- disable_query_log
-- disable_result_log
let $n=25000;
while ($n)
{
  eval insert into t1 values($n,$n);
  dec $n;
}
-- enable_result_log
-- enable_query_log
-- echo # Verify that the insertion to the table works fine after truncation.
select count(*) from t1;
drop table t1;


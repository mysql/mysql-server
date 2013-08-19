# Need more as 16k open files per process (ulimit -n) on Unix systems.
--source include/have_partition_open_file_limit.inc
##### max rows to be inserted
let $maxrows=65535;

# Create a list partitioned table with 8192 partitions.
# The CREATE will be written into a temporary file ($MYSQL_TMP_DIR/part_list_elem.inc).
# After inserting the content of the file it will be removed.
perl;
   use strict;
   use warnings;
   my $fname= "$ENV{'MYSQL_TMP_DIR'}/part_list_elem.inc";
   my @wrlines;
   push (@wrlines, "eval create table t2 (a int) engine=\$engine\n");
   push (@wrlines, "     partition by list (a) (\n");
   for(my $i=0; $i<8192; $i++)
   {
     my $j= $i*4;
     if ($i<8191) 
        {
          my $pattern= "        PARTITION p$i VALUES IN (".$j++.",".$j++.",".$j++.",".$j++."),\n";
          push(@wrlines,$pattern);
        }
     else
        {
          my $last_pattern= "        PARTITION p$i VALUES IN (".$j++.",".$j++.",".$j++.",".$j++.")\n";
          push(@wrlines,$last_pattern);
        }
       
   }
   push (@wrlines, "     );\n");
   open(FILE, ">", $fname) or die;
   print FILE @wrlines;
   close FILE;
EOF

source $MYSQL_TMP_DIR/part_list_elem.inc;
remove_file $MYSQL_TMP_DIR/part_list_elem.inc;

let $count= 32767;
--echo $count inserts;
--disable_query_log
while ($count)
{
eval insert into t2 values ($count);
dec $count;
}
--enable_query_log
select count(*) from t2;
select count(*) from t2 partition (p0);
select count(*) from t2 partition (p10);
select count(*) from t2 partition (p100);
select count(*) from t2 partition (p1000);
select count(*) from t2 partition (p4000);
select count(*) from t2 partition (p8000);
select count(*) from t2 partition (p8191);

select * from t2 partition (p0);
select * from t2 partition (p10);
select * from t2 partition (p100);
select * from t2 partition (p1000);
select * from t2 partition (p4000);
select * from t2 partition (p8000);
select * from t2 partition (p8191);

delete from t2 partition (p8191);
select * from t2 partition (p8191);
insert into t2 partition (p8191) values (32766),(32767);
select * from t2 partition (p8191);
update t2 partition (p8191) set a=32765 where a= 32766;
select * from t2 partition (p8191);

write_file $MYSQL_TMP_DIR/data01;
32766,
32764,
EOF
replace_result $MYSQL_TMP_DIR MYSQL_TMP_DIR;
eval load data infile '$MYSQL_TMP_DIR/data01' into table t2 partition (p8191) fields terminated by ',';
remove_file $MYSQL_TMP_DIR/data01;
select * from t2 partition (p8191);

drop table t2;


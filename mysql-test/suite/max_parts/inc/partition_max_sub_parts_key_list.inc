# Need more as 16k open files per process (ulimit -n) on Unix systems.
--source include/have_partition_open_file_limit.inc
# Create a list partitioned table with 8192 partitions.
# The CREATE will be written into a temporary file ($MYSQL_TMP_DIR/part_list_elem.inc).
# After inserting the content of the file it will be removed.
perl;
   use strict;
   use warnings;
   my $fname= "$ENV{'MYSQL_TMP_DIR'}/part_list_elem.inc";
   my @wrlines;
   push (@wrlines, "eval create table t2 (a int not null, primary key(a)) engine=\$engine\n");
   push (@wrlines, "     partition by list (a)\n");
   push (@wrlines, "     subpartition by key (a)");
   push (@wrlines, "     subpartitions 2 (\n");
   for(my $i=0; $i<4096; $i++)
   {
     my $j= $i*4;
     if ($i<4095) 
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
#remove_file $MYSQL_TMP_DIR/part_list_elem.inc;

let $count= 16383;
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
select count(*) from t2 partition (p4095);

select * from t2 partition (p0);
select * from t2 partition (p10);
select * from t2 partition (p100);
select * from t2 partition (p1000);
select * from t2 partition (p4000);
select * from t2 partition (p4095);

delete from t2 partition (p4095);
select * from t2 partition (p4095);
insert into t2 partition (p4095) values (16382),(16383);
select * from t2 partition (p4095);
update t2 partition (p4095) set a=16381 where a= 16382;
select * from t2 partition (p4095);

write_file $MYSQL_TMP_DIR/data01;
16380,
16382,
EOF
replace_result $MYSQL_TMP_DIR MYSQL_TMP_DIR;
eval load data infile '$MYSQL_TMP_DIR/data01' into table t2 partition (p4095) fields terminated by ',';
remove_file $MYSQL_TMP_DIR/data01;
select * from t2 partition (p4095);

error ER_TOO_MANY_PARTITIONS_ERROR;
alter table t2 add partition (partition p4096 values in (16384,16385,16386,16387));

drop table t2;


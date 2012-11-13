# Need more as 16k open files per process (ulimit -n) on Unix systems.
--source include/have_partition_open_file_limit.inc
##### max rows to be inserted
let $maxrows=65535;

error ER_PARSE_ERROR;
eval create table t2 (a int not null, primary key(a)) engine=$engine
partition by key (a) partitions ;

error ER_PARSE_ERROR;
eval create table t2 (a int not null, primary key(a)) engine=$engine
partition by key (a) partitions ten;

error ER_PARSE_ERROR;
eval create table t2 (a int not null, primary key(a)) engine=$engine
partition by key (a) partitions 24.56;

error ER_PARSE_ERROR;
eval create table t2 (a int not null, primary key(a)) engine=$engine
partition by key (a) partitions -1;

error ER_NO_PARTS_ERROR;
eval create table t2 (a int not null, primary key(a)) engine=$engine
partition by key (a) partitions 0;

error ER_TOO_MANY_PARTITIONS_ERROR;
eval create table t2 (a int not null, primary key(a)) engine=$engine
partition by key (a) partitions 8193;

error ER_PARSE_ERROR;
eval create table t2 (a int not null, primary key(a)) engine=$engine
partition by hash (a) partitions;

error ER_PARSE_ERROR;
eval create table t2 (a int not null, primary key(a)) engine=$engine
partition by hash (a) partitions ten;

error ER_PARSE_ERROR;
eval create table t2 (a int not null, primary key(a)) engine=$engine
partition by hash (a) partitions 24.56;

error ER_PARSE_ERROR;
eval create table t2 (a int not null, primary key(a)) engine=$engine
partition by hash (a) partitions -1;

error ER_NO_PARTS_ERROR;
eval create table t2 (a int not null, primary key(a)) engine=$engine
partition by hash (a) partitions 0;

error ER_TOO_MANY_PARTITIONS_ERROR;
eval create table t2 (a int not null, primary key(a)) engine=$engine
partition by hash (a) partitions 8193;

# Create a range partitioned table with 8193 partitions.
# The CREATE will be written into a temporary file ($MYSQL_TMP_DIR/part_list_elem.inc).
# After inserting the content of the file it will be removed.
perl;
   use strict;
   use warnings;
   my $fname= "$ENV{'MYSQL_TMP_DIR'}/part_list_elem.inc";
   my @wrlines;
   push (@wrlines, "error ER_TOO_MANY_PARTITIONS_ERROR;\n");
   push (@wrlines, "eval create table t2 (a int) engine=\$engine\n");
   push (@wrlines, "     partition by range (a) (\n");
   for(my $i=0; $i<8193; $i++)
   {
     my $j= ($i+1)* 8;
     my $pattern= "        PARTITION p$i VALUES LESS THAN ($j),\n";
     my $last_pattern= "        PARTITION p$i VALUES LESS THAN ($j)\n";
     if ($i<8192) 
        {
          push(@wrlines,$pattern);
        }
     else
        {
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


# Create a range partitioned table with 8193 partitions.
# The CREATE will be written into a temporary file ($MYSQL_TMP_DIR/part_list_elem.inc).
# After inserting the content of the file it will be removed.
perl;
   use strict;
   use warnings;
   my $fname= "$ENV{'MYSQL_TMP_DIR'}/part_list_elem.inc";
   my @wrlines;
   push (@wrlines, "error ER_TOO_MANY_PARTITIONS_ERROR;\n");
   push (@wrlines, "eval create table t2 (a int) engine=\$engine\n");
   push (@wrlines, "     partition by list (a) (\n");
   for(my $i=0; $i<8193; $i++)
   {
     my $j= $i*4;
     if ($i<8192) 
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

error ER_TOO_MANY_PARTITIONS_ERROR;
source $MYSQL_TMP_DIR/part_list_elem.inc;
remove_file $MYSQL_TMP_DIR/part_list_elem.inc;


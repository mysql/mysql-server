# Verify that contents of directory for databases
# ndb_ddl_test and ndb_ddkl_test2 match on all mysqld
echo == verify_mysql_dd.inc ==;

# First create the reference
--connection mysqld1
let $ref_file=$MYSQLTEST_VARDIR/tmp/ndb_ddl_ref.txt;
let $data_dir=`select @@datadir`;
--list_files_write_file $ref_file $data_dir/ndb_ddl_test
--list_files_append_file $ref_file $data_dir/ndb_ddl_test2

# Then iterate all mysqlds and check the contents of
# the database directory is the same
let $i = $NUM_MYSQLDS;
while($i)
{
  --connection mysqld$i
  let $file=$MYSQLTEST_VARDIR/tmp/ndb_ddl_$i.txt;
  let $data_dir=`select @@datadir`;
  --list_files_write_file $file $data_dir/ndb_ddl_test
  --list_files_append_file $file $data_dir/ndb_ddl_test2
  --diff_files $ref_file $file
  --remove_file $file

  dec $i;
}
--remove_file $ref_file

connection default;


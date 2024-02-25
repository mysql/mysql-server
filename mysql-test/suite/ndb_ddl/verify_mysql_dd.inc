# Verify that contents of directory for databases
# ndb_ddl_test and ndb_ddl_test2 match on all mysqld
#

echo == verify_mysql_dd.inc ==;

#
# Create empty file, serving as reference to show that the physical
# data directory on disk is now empty.
#
let $empty_ref_file=$MYSQLTEST_VARDIR/tmp/ndb_ddl_empty_ref.txt;
--write_file $empty_ref_file
EOF


# First create the reference file by selecting
# the compare query into ref_file
--connection mysqld1
let $ref_file=$MYSQLTEST_VARDIR/tmp/ndb_ddl_ref.txt;
# Build query used to compare the tables in ndb_ddl_test and
# ndb_ddl_test2 databases between all the servers, the query
# selects from the DD by using information_schema tables.
let $cmp_query=
  select TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS,
         AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH,
         DATA_FREE, AUTO_INCREMENT, TABLE_COLLATION, CHECKSUM,
         CREATE_OPTIONS, TABLE_COMMENT
    from information_schema.tables
    where TABLE_SCHEMA = 'ndb_ddl_test' or
          TABLE_SCHEMA = 'ndb_ddl_test2'
    order by TABLE_SCHEMA, TABLE_NAME;
#echo cmp_query: $cmp_query;
--disable_query_log ONCE
eval $cmp_query into outfile '$ref_file';

# Then iterate all mysqlds and check conditions
let $i = $NUM_MYSQLDS;
while($i)
{
  --connection mysqld$i

  # Check that output from compare query is the
  # same as the reference file
  let $file=$MYSQLTEST_VARDIR/tmp/ndb_ddl_$i.txt;
  --disable_query_log ONCE
  eval $cmp_query into outfile '$file';
  --diff_files $ref_file $file
  --remove_file $file

  # Check that there are no files in the data directory
  # by listing files and compare with the empty file
  let $empty_file=$MYSQLTEST_VARDIR/tmp/ndb_ddl_empty_$i.txt;
  let $data_dir=`select @@datadir`;
  --list_files_write_file $empty_file $data_dir/ndb_ddl_test
  --list_files_append_file $empty_file $data_dir/ndb_ddl_test2
  --diff_files $empty_ref_file $empty_file
  --remove_file $empty_file

  dec $i;
}
--remove_file $ref_file
--remove_file $empty_ref_file

connection default;


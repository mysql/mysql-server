#
# Select the contents of each (sub)partition.
#
# Input: $schema: Schema name (defaults to 'test').
# Input: $table:  Table name.
#

--disable_query_log
if (!$schema)
{
  --let $schema= test
}

# Create a temporary table containing partition names.
eval CREATE TEMPORARY TABLE partition_names AS
  SELECT IFNULL(subpartition_name, partition_name)
    FROM INFORMATION_SCHEMA.PARTITIONS
    WHERE TABLE_NAME = '$table' AND
          TABLE_SCHEMA = '$schema';

# Open a handler to use as a cursor for the partition names.
HANDLER partition_names OPEN;
--let $partition_name= `HANDLER partition_names READ FIRST`
--enable_query_log

# Iterate over partition names and select contents of each.
while ($partition_name)
{
  --eval SELECT * FROM $schema.$table PARTITION($partition_name)
  --let $partition_name= `HANDLER partition_names READ NEXT`
}

# Close handler and drop temporary table.
--disable_query_log
HANDLER partition_names CLOSE;
DROP TABLE partition_names;
--enable_query_log

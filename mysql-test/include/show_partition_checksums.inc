#
# Compute MD5 checksums for the contents of each (sub)partition.
# Print partition name and checksum.
#
# Note: Concatenating column contents into a string is constrained
#       by max length given by 'group_concat_max_len' (default 1024).
#
# Note: GROUP_CONCAT() ignores rows having NULL values, so be careful
#       when checksumming such tables since modifying such a row may
#       not affect the checkusm. The script below will enclose
#       non-NULL values in double quotes, and add an unquoted string
#       NULL to the content string for values being NULL before
#       checksumming, unless the $columns variable is specified by the
#       user before invoking the script.
#
# Input: $schema:  Schema name (defaults to 'test').
# Input: $table:   Table name.
# Input: $columns: Comma separated list of column names (defaults
#                  to all columns).
#

--disable_query_log
if (!$schema)
{
  --let $schema= test
}

if (!$columns)
{
  let $columns= `SELECT GROUP_CONCAT(
                   CONCAT('IFNULL(',
                     CONCAT('CONCAT(\'\\\"\', ', COLUMN_NAME, ', \'\\\"\')'),
                     ', \'NULL\')'))
                   FROM INFORMATION_SCHEMA.COLUMNS
                     WHERE TABLE_NAME = '$table' AND
                           TABLE_SCHEMA = '$schema'`;
}

# Create a temporary table containing partition names.
eval CREATE TEMPORARY TABLE partition_names AS
  SELECT IFNULL(subpartition_name, partition_name)
    FROM INFORMATION_SCHEMA.PARTITIONS
    WHERE TABLE_NAME = '$table' AND
          TABLE_SCHEMA = '$schema';

# Create a temporary table containing checksums.
CREATE TEMPORARY TABLE checksums(`partition` VARCHAR(64),
                                 checksum VARCHAR(32));

# Open a handler to use as a cursor for the partition names.
HANDLER partition_names OPEN;
--let partition_name= `HANDLER partition_names READ FIRST`

# Iterate over partition names and select contents of each.
while ($partition_name)
{
  eval INSERT INTO checksums
    SELECT '$partition_name', MD5(GROUP_CONCAT($columns))
      FROM $schema.$table PARTITION($partition_name);
  --let partition_name= `HANDLER partition_names READ NEXT`
}

# Print checksums, close handler and drop temporary table.
SELECT * FROM checksums;
HANDLER partition_names CLOSE;
DROP TABLE partition_names;
DROP TABLE checksums;
--enable_query_log

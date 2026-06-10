# Create tables storing the contents of the JDV I_S views.
# Compute checksums for each table, concat the checksums into a string.
# Alternate between storing the checksum in an mtr variable and
# asserting that the current checksum is the same as the previous
#

--disable_query_log

# Disable binlogging of temporary tables when binlog_format=STATEMENT
--source include/rpl/disable_binlog.inc
CREATE TEMPORARY TABLE checksum_views AS SELECT * FROM INFORMATION_SCHEMA.JSON_DUALITY_VIEWS;
CREATE TEMPORARY TABLE checksum_tables AS SELECT * FROM INFORMATION_SCHEMA.JSON_DUALITY_VIEW_TABLES;
CREATE TEMPORARY TABLE checksum_columns AS SELECT * FROM INFORMATION_SCHEMA.JSON_DUALITY_VIEW_COLUMNS;
CREATE TEMPORARY TABLE checksum_links AS SELECT * FROM INFORMATION_SCHEMA.JSON_DUALITY_VIEW_LINKS;

--let $prev_checksum = $checksum

--let $checksum_views   = `CHECKSUM TABLE checksum_views EXTENDED`
--let $checksum_tables  = `CHECKSUM TABLE checksum_tables EXTENDED`
--let $checksum_columns = `CHECKSUM TABLE checksum_columns EXTENDED`
--let $checksum_links   = `CHECKSUM TABLE checksum_links EXTENDED`
--let $checksum = $checksum_views $checksum_tables $checksum_columns $checksum_links

# If there was a previous checksum, assume we're on the replica. Then,
# assert the new checksum is the same as the previous, and reset the new
# checksum.
if ($prev_checksum)
{
  --assert($checksum == $prev_checksum)
  --let $checksum =
}

DROP TABLE checksum_views, checksum_tables, checksum_columns, checksum_links;
# Restore binlogging
--source include/rpl/restore_binlog.inc
--enable_query_log


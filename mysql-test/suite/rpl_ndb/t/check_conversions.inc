--echo **** Checking result of conversions ****
disable_query_log;
# Create a temporary table containing all conversions
# that didn't work as expected, the table should
# normally not contain any rows
CREATE TEMPORARY TABLE failed_type_conversions ENGINE = 'MyISAM' AS
  SELECT RPAD(Source, 15, ' ') AS Source_Type,
         RPAD(Target, 15, ' ') AS Target_Type,
         RPAD(Flags, 25, ' ') AS All_Type_Conversion_Flags,
         IF(Compare IS NULL AND Error IS NOT NULL, '<Correct error>',
            IF(Compare, '<Correct value>',
               CONCAT("'", On_Slave, "' != '", Expected, "'")))
           AS Value_On_Slave
  FROM type_conversions
  WHERE Compare != 1 OR (Compare IS NULL AND Error IS NULL);
enable_query_log;

if (`SELECT COUNT(*) FROM failed_type_conversions`)
{
  echo Some type conversions failed, see below:;
  SELECT * FROM failed_type_conversions;
  die Type conversion failure;
}




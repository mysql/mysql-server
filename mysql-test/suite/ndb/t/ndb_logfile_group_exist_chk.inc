# Check whether a logfile group exists. Can be used
# either at the end of a test to check the cleanup succeeded
# or at the beginning of a test to check whether it starts with
# a clean slate.

let $cmd= SELECT count(*) FROM INFORMATION_SCHEMA.FILES
WHERE FILE_TYPE = 'UNDO LOG' AND FILE_NAME IS NOT NULL AND
  LOGFILE_GROUP_NAME IS NOT NULL
GROUP BY LOGFILE_GROUP_NAME, FILE_NAME, ENGINE, TOTAL_EXTENTS,
  INITIAL_SIZE, EXTRA
ORDER BY LOGFILE_GROUP_NAME;

let $result= `$cmd`;

if ($result)
{
  echo;
  echo Logfile group exists: $result;
  echo;
  SELECT LOGFILE_GROUP_NAME, FILE_NAME, TOTAL_EXTENTS,
    INITIAL_SIZE, ENGINE, EXTRA
  FROM INFORMATION_SCHEMA.FILES
  WHERE FILE_TYPE = 'UNDO LOG' AND FILE_NAME IS NOT NULL AND
    LOGFILE_GROUP_NAME IS NOT NULL
  GROUP BY LOGFILE_GROUP_NAME, FILE_NAME, ENGINE, TOTAL_EXTENTS,
    INITIAL_SIZE, EXTRA
  ORDER BY LOGFILE_GROUP_NAME;

  die Error: Logfile group exists already, please check. Exiting the test;
}

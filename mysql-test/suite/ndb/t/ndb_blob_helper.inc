# Helper for checking cost of statement
# execution
# Parameters
# $tablename
# $valCol1
# $valCol2
# $sql_statement

# Preparation
--disable_query_log
--disable_result_log
eval delete from $tablename;

if (!$valCol2)
{
  --let $short_ins = repeat("B",20)
  --let $long_ins = repeat("J", 5000)
}
if ($valCol2)
{
  --let $short_ins = repeat("C",20), repeat("T", 20)
  --let $long_ins = repeat("S", 5000), repeat("G", 5000)
}
eval insert into $tablename values 
  ("Short1", $short_ins),
  ("Short2", $short_ins),
  ("Short3", $short_ins),
  ("Long1", $long_ins),
  ("Long2", $long_ins),
  ("Long3", $long_ins);

--enable_query_log
--enable_result_log

# Now run the statement
--source suite/ndb/include/ndb_init_api_counts.inc
--eval $sql_statement
--source suite/ndb/include/ndb_diff_api_counts.inc

--disable_query_log
--disable_result_log
commit;
--enable_query_log
--enable_result_log
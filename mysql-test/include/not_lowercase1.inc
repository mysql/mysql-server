# ====== Purpose ======
#
# Ensure that the server is not running with
# --lower_case_table_names=1 
#
# ====== Usage ======
# 
# --source include/not_lowercase1.inc

--disable_warnings
if (`SELECT VARIABLE_VALUE=1 FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE VARIABLE_NAME = 'LOWER_CASE_TABLE_NAMES'`)
{
  --skip --lower_case_table_names=1 is not valid for this test.
}
--enable_warnings


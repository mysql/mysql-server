# Purpose:
# To get error number from the error code.
# Every message added in the the error log file has an error number. At times,
# we grep error message in the log file for verification purpose. It is
# cumbersome to write the REGEX pattern in the test to do the same. Moreover,
# when format or content of an error message change it requires to fix the test
# again. We can avoid all of these problems by grepping error id in the test.
#
# Parameters:
#  --let $error_name=<Error Code>
#
# Usage:
# --let $error_name=ER_IB_MSG_DBLWR_LOAD_WRONG_SIZE
# --source include/get_error_id.inc
#
# --let SEARCH_PATTERN=$error_id
# --source include/search_pattern.inc
#         OR
# --let SEARCH_PATTERN=$error_id<REGEX_PATTERN>
# --source include/search_pattern.inc

if (!$error_name)
{
  --die This script requires valid parameter 'error_name'
}

--let $error_id=convert_error($error_name)
--let $error_lpadded_id=$error_id
--let $error_id_pumped=$error_id
#The format of errors is MY-000000, so we have to LPAD with zeros
#if the error_id is shorter than 6 digits.
while($error_id_pumped<100000)
{
  --let $error_lpadded_id=0$error_lpadded_id
  --let $TEN = 10
  --expr $error_id_pumped = $error_id_pumped * $TEN
}
--let $error_id=MY-$error_lpadded_id

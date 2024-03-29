#### suite/funcs_1/include/show_connection.inc
#
# This file shows the current connection information into the result file
# By using the '... AS "" ' an empty line will separate this in the result.
#
# Usage:
#    Add the following to any *.test file:
#      :
#    --source suite/funcs_1/include/show_connection.inc
#      :

--disable_query_log
SELECT USER() AS "", DATABASE() AS "";
--enable_query_log

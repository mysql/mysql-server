# ==== Purpose ====
#
# This waits for the file open_count to reach
# zero for the file name provided as a parameter.
#
# ==== Usage ====
#
# --let $wait_file_name=XXX
# --source include/wait_for_file_closed.inc
#

--let $include_filename= wait_for_file_closed.inc
--source include/begin_include_file.inc

--let $wait_condition= SELECT OPEN_COUNT=0 FROM performance_schema.file_instances WHERE FILE_NAME LIKE "%$wait_file_name"
--source include/wait_condition.inc

--source include/end_include_file.inc

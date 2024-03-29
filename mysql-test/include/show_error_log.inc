# ==== Purpose ====
#
# Show the tail of error log generated since the last invocation of
# save_error_log_position.inc
#
# By default, this script invokes save_error_log_position.inc at the
# end, so you can easily use it with a sequence of error-generating
# statements, like:
#
#   --source include/save_error_log_position.inc
#   <generate some error>
#   --source include/show_error_log.inc
#   <generate some error>
#   --source include/show_error_log.inc
#   ...
#
# ==== Usage ====
#
# [--let $error_log_position = TIMESTAMP]
# [--let $error_log_columns = COLUMNS]
# [--let $save_error_log_position = 0]
# --source include/show_error_log.inc
#

--let $include_filename = show_error_log.inc
--source include/begin_include_file.inc

--let $error_pattern = SHOW
--source include/assert_error_log.inc

--let $include_filename = show_error_log.inc
--source include/end_include_file.inc

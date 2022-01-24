# ==== Purpose ====
#
# Store the current position of the error log in a variable.  This is
# intended to be used in conjunction with include/show_error_log.inc
# or include/assert_error_log.inc
#
# ==== Usage ====
#
# [--let $error_log_server_number = NUMBER]
# --source include/save_error_log_position.inc
#
# Parameters:
#
#   $error_log_server_number
#
#     By default, this script operates on the error log for the server
#     of the current connection.  To use this script without sending
#     queries on the current connection, set this variable to the
#     server number instead.

--let $include_filename = save_error_log_position.inc
--source include/begin_include_file.inc

--let $error_pattern = SAVEPOS
--source include/assert_error_log.inc

--let $include_filename = save_error_log_position.inc
--source include/end_include_file.inc

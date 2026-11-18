# ==== Purpose ====
#
# Set $spill_dir to the in-memory relay log spill directory of the server on
# the current connection:
#
#   <directory of relay_log_basename>/#in_memory_relaylog_temp_files.<leaf>
#
# where <leaf> is the file-name part of @@GLOBAL.relay_log_basename. Each
# server has its own spill directory, shared by all its channels.
#
# ==== Usage ====
#
# --source include/rpl/get_imr_spill_dir.inc
#
# Output variable:
#
#   $spill_dir
#     The spill directory path, without a trailing separator. It always
#     uses '/' as the directory separator, also on Windows, where the
#     server reports relay_log_basename with '\'. mysqltest file commands
#     and Perl accept '/' on Windows, and '/' needs no escaping when the
#     path is interpolated into an SQL string literal.
#
--let $spill_dir= `SELECT CONCAT(LEFT(b, LENGTH(b) - LOCATE('/', REVERSE(b))), '/#in_memory_relaylog_temp_files.', SUBSTRING_INDEX(b, '/', -1)) FROM (SELECT REPLACE(@@GLOBAL.relay_log_basename, CHAR(92), '/') AS b) AS t`

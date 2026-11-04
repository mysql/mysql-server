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
#     The spill directory path, without a trailing separator.
#
--let $spill_dir= `SELECT CONCAT(LEFT(@@GLOBAL.relay_log_basename, LENGTH(@@GLOBAL.relay_log_basename) - LOCATE('/', REVERSE(@@GLOBAL.relay_log_basename))), '/#in_memory_relaylog_temp_files.', SUBSTRING_INDEX(@@GLOBAL.relay_log_basename, '/', -1))`

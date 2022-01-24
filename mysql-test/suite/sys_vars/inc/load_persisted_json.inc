# ==== Purpose ====
#
# Read the contents of the persisted variables into an mtr variable.
# The file contains a JSON document. This script loads the file that
# belongs to the server of the current connection.
#
# ==== Usage ====
#
# [--let $escape_persisted_json = 1]
# --source include/load_persisted_json.inc
# # Now, the mtr variable $persisted_json contains the contents of the file.
#
# Parameters:
#
#   $escape_persisted_json
#     If set, escape single quotes and backslashes in $persisted_json
#     so that it can be used in string context in subsequent queries.

--let $persisted_json = `SELECT LOAD_FILE(CONCAT(@@global.datadir, 'mysqld-auto.cnf'))`
if ($escape_persisted_json) {
  --let $persisted_json = escape(\',$persisted_json)
}

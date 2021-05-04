# ==== Purpose ====
#
# Restart the server
#
# ==== Usage ====
#
# --source include/keyring_tests/cleanup_server_with_manifest.inc
#

--echo # Restarting server without the manifest file
--source include/force_restart.inc

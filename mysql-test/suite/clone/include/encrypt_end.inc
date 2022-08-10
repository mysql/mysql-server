--echo # Restart server and remove encrypted data directory
let $restart_parameters = "restart:";
--source include/restart_mysqld.inc

# Temporary file Cleanup
--force-rmdir $ENCRYPT_DATADIR
--remove_file $ENCRYPT_ERROR_FILE
--remove_file $BOOTSTRAP_SQL
--remove_file $MYSQL_TMP_DIR/mysecret_keyring

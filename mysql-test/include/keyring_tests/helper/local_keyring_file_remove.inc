# ==== Purpose ====
#
# Remove keyring file for given keyring component
#
# ==== Usage ====
#
# --let COMPONENT_NAME = <name_of_keyring_component>
# --let KEYRING_FILE_PATH = <path_of_keyring_file>
# --source include/keyring_tests/local_keyring_file_remove.inc
#
# ==== Parameters ====
#
# $COMPONENT_NAME
#    Name of the keyring component.
#
# $KEYRING_FILE_PATH
#    Path to keyring file
#

--echo # Removing local keyring file for keyring component: $COMPONENT_NAME
--error 0,1
--remove_file $KEYRING_FILE_PATH

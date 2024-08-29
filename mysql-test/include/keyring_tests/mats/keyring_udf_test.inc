--echo # ----------------------------------------------------------------------
--echo # Setting up Keyring UDFs

--source include/keyring_tests/helper/install_keyring_udf.inc

--echo # ----------------------------------------------------------------------

--echo # Tests for AES key type
--source keyring_udf_aes_test.inc

--echo # Tests for SECRET key type
--source keyring_udf_secret_test.inc

--echo # ----------------------------------------------------------------------

--echo # Clean-up
--source include/keyring_tests/helper/uninstall_keyring_udf.inc
--echo # ----------------------------------------------------------------------

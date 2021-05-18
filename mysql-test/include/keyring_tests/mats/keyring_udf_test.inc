--echo # ----------------------------------------------------------------------
--echo # Setting up Keyring UDFs

# Install Keyring UDF plugin
--replace_regex /\.dll/.so/
--eval INSTALL PLUGIN keyring_udf SONAME '$KEYRING_UDF'

--replace_regex /\.dll/.so/
--eval CREATE FUNCTION keyring_key_store RETURNS INTEGER SONAME '$KEYRING_UDF'
--replace_regex /\.dll/.so/
--eval CREATE FUNCTION keyring_key_generate RETURNS INTEGER SONAME '$KEYRING_UDF'
--replace_regex /\.dll/.so/
--eval CREATE FUNCTION keyring_key_fetch RETURNS sTRING SONAME '$KEYRING_UDF'
--replace_regex /\.dll/.so/
--eval CREATE FUNCTION keyring_key_type_fetch RETURNS STRING SONAME '$KEYRING_UDF'
--replace_regex /\.dll/.so/
--eval CREATE FUNCTION keyring_key_length_fetch RETURNS INTEGER SONAME '$KEYRING_UDF'
--replace_regex /\.dll/.so/
--eval CREATE FUNCTION keyring_key_remove RETURNS INTEGER SONAME '$KEYRING_UDF'

--echo # ----------------------------------------------------------------------

--echo # Tests for AES key type
--source keyring_udf_aes_test.inc

--echo # Tests for SECRET key type
--source keyring_udf_secret_test.inc

--echo # ----------------------------------------------------------------------

--echo # Clean-up
DROP FUNCTION keyring_key_store;
DROP FUNCTION keyring_key_fetch;
DROP FUNCTION keyring_key_remove;
DROP FUNCTION keyring_key_generate;
DROP FUNCTION keyring_key_type_fetch;
DROP FUNCTION keyring_key_length_fetch;
UNINSTALL PLUGIN keyring_udf;
--echo # ----------------------------------------------------------------------

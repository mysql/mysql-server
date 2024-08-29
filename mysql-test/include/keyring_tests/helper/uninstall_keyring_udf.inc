# Uninstall Keyring UDF plugin
DROP FUNCTION keyring_key_store;
DROP FUNCTION keyring_key_fetch;
DROP FUNCTION keyring_key_remove;
DROP FUNCTION keyring_key_generate;
DROP FUNCTION keyring_key_type_fetch;
DROP FUNCTION keyring_key_length_fetch;
UNINSTALL PLUGIN keyring_udf;


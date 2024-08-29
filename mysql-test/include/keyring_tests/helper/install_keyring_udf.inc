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


--echo # create encrypted table with master key generated from source keyring plugin
CREATE TABLE T(i INT) ENCRYPTION='Y';
INSERT INTO T VALUES (9769);
SELECT * FROM T;

--replace_regex /\.dll/.so/
eval INSTALL PLUGIN keyring_udf SONAME '$KEYRING_UDF';
--replace_regex /\.dll/.so/
eval CREATE FUNCTION keyring_key_generate RETURNS INTEGER SONAME '$KEYRING_UDF';
--replace_regex /\.dll/.so/
eval CREATE FUNCTION keyring_key_store RETURNS INTEGER SONAME '$KEYRING_UDF';
--replace_regex /\.dll/.so/
eval CREATE FUNCTION keyring_key_fetch RETURNS STRING SONAME '$KEYRING_UDF';
--replace_regex /\.dll/.so/
eval CREATE FUNCTION keyring_key_type_fetch RETURNS STRING SONAME '$KEYRING_UDF';
--replace_regex /\.dll/.so/
eval CREATE FUNCTION keyring_key_length_fetch RETURNS INTEGER SONAME '$KEYRING_UDF';
--replace_regex /\.dll/.so/
eval CREATE FUNCTION keyring_key_remove RETURNS INTEGER SONAME '$KEYRING_UDF';

--let $key_name1=`select concat("key_", current_timestamp(6))`
--replace_regex /[0-9]*-[0-9]*-[0-9]* [0-9]*:[0-9]*:[0-9]*.[0-9]*/key1_timestamp/
--eval SELECT keyring_key_generate('$key_name1','AES',16)
--let $key_name2=`select concat("key_", current_timestamp(6))`
--replace_regex /[0-9]*-[0-9]*-[0-9]* [0-9]*:[0-9]*:[0-9]*.[0-9]*/key2_timestamp/
--eval SELECT keyring_key_generate('$key_name2','AES',24)
--let $key_name3=`select concat("key_", current_timestamp(6))`
--replace_regex /[0-9]*-[0-9]*-[0-9]* [0-9]*:[0-9]*:[0-9]*.[0-9]*/key3_timestamp/
--eval SELECT keyring_key_generate('$key_name3','AES',32)

let $wl9769_source_key1 = `SELECT hex(keyring_key_fetch('$key_name1'))`;
let $wl9769_source_key2 = `SELECT hex(keyring_key_fetch('$key_name2'))`;
let $wl9769_source_key3 = `SELECT hex(keyring_key_fetch('$key_name3'))`;

let $wl9769_source_key1_type = `SELECT keyring_key_type_fetch('$key_name1')`;
let $wl9769_source_key2_type = `SELECT keyring_key_type_fetch('$key_name2')`;
let $wl9769_source_key3_type = `SELECT keyring_key_type_fetch('$key_name3')`;

let $wl9769_source_key1_len = `SELECT keyring_key_length_fetch('$key_name1')`;
let $wl9769_source_key2_len = `SELECT keyring_key_length_fetch('$key_name2')`;
let $wl9769_source_key3_len = `SELECT keyring_key_length_fetch('$key_name3')`;

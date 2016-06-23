--replace_regex /\.dll/.so/
eval create function keyring_key_store returns integer soname '$KEYRING_UDF';
--replace_regex /\.dll/.so/
eval create function keyring_key_fetch returns string soname '$KEYRING_UDF';
--replace_regex /\.dll/.so/
eval create function keyring_key_type_fetch returns string soname '$KEYRING_UDF';
--replace_regex /\.dll/.so/
eval create function keyring_key_length_fetch returns integer soname '$KEYRING_UDF';
--replace_regex /\.dll/.so/
eval create function keyring_key_remove returns integer soname '$KEYRING_UDF';
--replace_regex /\.dll/.so/
eval create function keyring_key_generate returns integer soname '$KEYRING_UDF';

--replace_regex /\.dll/.so/
select * from mysql.func where name like '%keyring%' order by name;

--error ER_CANT_INITIALIZE_UDF
select keyring_key_generate('Rob_AES_128','AES',16);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_store('Rob_AES_128','AES',"0123456789012345");
--error ER_CANT_INITIALIZE_UDF
select keyring_key_fetch('Rob_AES_128');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_type_fetch('Rob_AES_128');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_length_fetch('Rob_AES_128');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_remove('Rob_AES_128');

drop function keyring_key_store;
drop function keyring_key_fetch;
drop function keyring_key_type_fetch;
drop function keyring_key_length_fetch;
drop function keyring_key_remove;
drop function keyring_key_generate;

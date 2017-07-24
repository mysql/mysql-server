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

# Error cases -- wrong argument count
--error ER_CANT_INITIALIZE_UDF
select keyring_key_store('Key_1');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_store('Key_1','AES');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_fetch('Key_1','AES');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_type_fetch('Key_1','AES');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_length_fetch('Key_1','AES');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_remove('Key_1','AES');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_generate('Key_1');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_generate('Key_1','AES');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_generate('Key_1','');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_fetch('Key_1',NULL);
# End of wrong count

# Error cases -- wrong argument type
--error ER_CANT_INITIALIZE_UDF
select keyring_key_store('Key_1','AES',123);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_store(NULL,'AES',123);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_store('Key_1',NULL,123);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_store('Key_1','AES',NULL);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_store(NULL,NULL,NULL);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_store(1234,NULL,'53247@#$%^');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_store(1,'AES','123');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_store('Key_1',123,'123');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_fetch(1);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_fetch(NULL);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_type_fetch(1);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_type_fetch(NULL);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_length_fetch(1);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_length_fetch(NULL);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_generate('Key_1','AES','123');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_generate(NULL,'AES','123');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_generate('Key_1',NULL,'123');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_generate('Key_1','AES',NULL);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_generate(NULL,NULL,NULL);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_generate(1,'AES',123);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_generate('Key_1',123,'123');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_generate('Key_invalid_key_type',123,123);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_remove(1);
--error ER_CANT_INITIALIZE_UDF
select keyring_key_remove(NULL);
# End of wrong argument type

#Error case -- too long key
--error ER_CANT_INITIALIZE_UDF
select keyring_key_generate('Rob_AES_8196','AES',2049);

#Check the plugins

select PLUGIN_NAME,PLUGIN_AUTHOR from information_schema.plugins where PLUGIN_NAME like '%keyring%';

# Genuine cases

# AES
# Generate AES_128
select keyring_key_generate('Rob_AES_128','AES',16);
select keyring_key_fetch('Rob_AES_128') into @x;
select LENGTH(@x);
select keyring_key_type_fetch('Rob_AES_128');
select keyring_key_length_fetch('Rob_AES_128');
select keyring_key_remove('Rob_AES_128');
# Store AES_128
select keyring_key_store('Rob_AES_128','AES',"0123456789012345");
select keyring_key_fetch('Rob_AES_128') into @x;
select LENGTH(@x);
select keyring_key_type_fetch('Rob_AES_128');
select keyring_key_length_fetch('Rob_AES_128');
select keyring_key_remove('Rob_AES_128');
# Generate AES_192
select keyring_key_generate('Rob_AES_192','AES',24);
select keyring_key_fetch('Rob_AES_192') into @x;
select LENGTH(@x);
select keyring_key_type_fetch('Rob_AES_192');
select keyring_key_length_fetch('Rob_AES_192');
select keyring_key_remove('Rob_AES_192');
# Store AES_192
select keyring_key_store('Rob_AES_192','AES',"012345678901234567890%@3");
select keyring_key_fetch('Rob_AES_192') into @x;
select LENGTH(@x);
select keyring_key_type_fetch('Rob_AES_192');
select keyring_key_length_fetch('Rob_AES_192');
select keyring_key_remove('Rob_AES_192');
# Generate AES_256
select keyring_key_generate('Rob_AES_256','AES',32);
select keyring_key_fetch('Rob_AES_256') into @x;
select LENGTH(@x);
select keyring_key_type_fetch('Rob_AES_256');
select keyring_key_length_fetch('Rob_AES_256');
select keyring_key_remove('Rob_AES_256');
# Store AES_256
select keyring_key_store('Rob_AES_256','AES',"01234567890123456789012345678901");
select keyring_key_fetch('Rob_AES_256') into @x;
select LENGTH(@x);
select keyring_key_type_fetch('Rob_AES_256');
select keyring_key_length_fetch('Rob_AES_256');
select keyring_key_remove('Rob_AES_256');

# Fetch not existing:
select keyring_key_fetch('key') into @x;
select @x;
select keyring_key_type_fetch('key') into @x;
select @x;
select keyring_key_length_fetch('key') into @x;
select @x;

--echo #Testing with AES_ENCRYPT/AES_DECRYPT

select keyring_key_generate('AES_128','AES',16);
select AES_ENCRYPT('secret message', keyring_key_fetch('AES_128')) into @cipher;
select AES_DECRYPT(@cipher, keyring_key_fetch('AES_128'));

select keyring_key_remove('AES_128');

# Fetch non-existent key - we should get empty string as result
select keyring_key_fetch('Rob1');
# Fetch non-existent type
select keyring_key_type_fetch('Rob1');
# Fetch length not existing
select keyring_key_length_fetch('Rob1');

# Errors comming from keyring
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_fetch('') into @x;
select @x;
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_length_fetch('') into @x;
select @x;
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_type_fetch('') into @x;
select @x;
# Remove not existing
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_remove('Rob_not_existing') into @x;
select @x;
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_remove('') into @x;
select @x;
# Generate wrong key type
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_generate('Wrong_type','xxx', 16) into @x;
select @x;
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_generate('','AES', 16) into @x;
select @x;
# Store wrong key type
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_store('Wrong_type','xxx', '0123456789012345') into @x;
select @x;
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_store('','AES', '0123456789012345') into @x;
select @x;

--echo # Testing privileges

select keyring_key_generate('roots_key', 'AES',16);

create definer=root@localhost procedure shared_key() select keyring_key_fetch('roots_key');

CREATE USER user_execute_test@localhost;
connect(conn_no_execute, localhost, user_execute_test);

--error ER_CANT_INITIALIZE_UDF
select keyring_key_generate('Rob_AES_no_privilege','AES',16);

connection default;
disconnect conn_no_execute;
GRANT EXECUTE ON *.* TO 'user_execute_test'@'localhost';

connect(conn_with_execute, localhost, user_execute_test);

select keyring_key_generate('Rob_AES_no_privilege','AES',16);
select keyring_key_generate('another_Rob_AES_no_privilege','AES',16);

--echo #Check if user you can access another user's keys
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_remove('roots_key');

--echo #Check that the user is able to access the key via procedure
--replace_column 1 #
call shared_key();

connection default;

DROP PROCEDURE shared_key;

--echo #Check if a user can create a key with id that already exist but belongs
--echo #to another user.

select keyring_key_generate('Rob_AES_no_privilege','AES',16);

--echo #Check if user with larger privileges can access another user's key

--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_remove('another_Rob_AES_no_privilege');

--echo #Cleanup

select keyring_key_remove('Rob_AES_no_privilege');
select keyring_key_remove('roots_key');
connection conn_with_execute;
select keyring_key_remove('Rob_AES_no_privilege');
select keyring_key_remove('another_Rob_AES_no_privilege');

connection default;
disconnect conn_with_execute;
DROP USER 'user_execute_test'@'localhost';

# End of genuine cases


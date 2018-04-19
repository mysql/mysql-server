--replace_regex /\.dll/.so/
eval create function keyring_key_store returns integer soname '$KEYRING_UDF';

# Error cases -- wrong argument count
--error ER_CANT_INITIALIZE_UDF
select keyring_key_store('Key_1');
--error ER_CANT_INITIALIZE_UDF
select keyring_key_store('Key_1','AES');

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

# Store AES_128
select keyring_key_store('Rob_AES_128','AES',"0123456789012345");
select keyring_key_fetch('Rob_AES_128') into @x;
select LENGTH(@x);
select keyring_key_type_fetch('Rob_AES_128');
select keyring_key_length_fetch('Rob_AES_128');
select keyring_key_remove('Rob_AES_128');

# Store AES_192
select keyring_key_store('Rob_AES_192','AES',"012345678901234567890%@3");
select keyring_key_fetch('Rob_AES_192') into @x;
select LENGTH(@x);
select keyring_key_type_fetch('Rob_AES_192');
select keyring_key_length_fetch('Rob_AES_192');
select keyring_key_remove('Rob_AES_192');

# Store AES_256
select keyring_key_store('Rob_AES_256','AES',"01234567890123456789012345678901");
select keyring_key_fetch('Rob_AES_256') into @x;
select LENGTH(@x);
select keyring_key_type_fetch('Rob_AES_256');
select keyring_key_length_fetch('Rob_AES_256');
select keyring_key_remove('Rob_AES_256');

# Store RSA_1024
select keyring_key_store('Rob_RSA_1024','RSA',"01234567890123456789012345678901234567890123456789012345678901230123456789012345678901234567890123456789012345678901234567890123");
select keyring_key_fetch('Rob_RSA_1024') into @x;
select LENGTH(@x);
select keyring_key_type_fetch('Rob_RSA_1024');
select keyring_key_length_fetch('Rob_RSA_1024');
select keyring_key_remove('Rob_RSA_1024');

# Store DSA_1024
select keyring_key_store('Rob_DSA_1024','DSA',"01234567890123456789012345678901234567890123456789012345678901230123456789012345678901234567890123456789012345678901234567890123");
select keyring_key_fetch('Rob_DSA_1024') into @x;
select LENGTH(@x);
select keyring_key_type_fetch('Rob_DSA_1024');
select keyring_key_length_fetch('Rob_DSA_1024');
select keyring_key_remove('Rob_DSA_1024');

# Store wrong key type
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_store('Wrong_type','xxx', '0123456789012345') into @x;
select @x;
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_store('','AES', '0123456789012345') into @x;
select @x;


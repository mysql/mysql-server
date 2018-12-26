--echo #The keyring plugin was uninstalled, the keyring_udf functions should fail with
--echo #appropriate error message

--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_store('Rob_uninstalled','AES','123');
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_fetch('Rob_uninstalled');
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_type_fetch('Rob_uninstalled');
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_length_fetch('Rob_uninstalled');
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_remove('Rob_uninstalled');
--error ER_KEYRING_UDF_KEYRING_SERVICE_ERROR
select keyring_key_generate('Rob_uninstalled_generate','DSA',128);




--echo # keyring_key_generate tests
SELECT keyring_key_generate('AES_g1', 'AES', 16);
SELECT keyring_key_generate('AES_g2', 'AES', 32);
SELECT keyring_key_generate('AES_g3', 'AES', 64);

--echo # keyring_key_store tests
SELECT keyring_key_store('AES_s1', 'AES', 'Quick brown fox jumped over the lazy dog');
SELECT keyring_key_store('AES_s2', 'AES', 'Old MacDonald had a farm');
SELECT keyring_key_store('AES_s3', 'AES', 'Quis custodiet ipsos custodes');

--echo # keyring_key_fetch tests
SELECT keyring_key_fetch('AES_g1') INTO @aes_g1_fetched;
SELECT keyring_key_fetch('AES_g2') INTO @aes_g2_fetched;
SELECT keyring_key_fetch('AES_g3') INTO @aes_g3_fetched;
SELECT LENGTH(@aes_g1_fetched);
SELECT LENGTH(@aes_g2_fetched);
SELECT LENGTH(@aes_g3_fetched);

SELECT keyring_key_fetch('AES_s1');
SELECT keyring_key_fetch('AES_s2');
SELECT keyring_key_fetch('AES_s3');

--echo # keyring key_type_fetch tests
SELECT keyring_key_type_fetch('AES_g1');
SELECT keyring_key_type_fetch('AES_g2');
SELECT keyring_key_type_fetch('AES_g3');
SELECT keyring_key_type_fetch('AES_s1');
SELECT keyring_key_type_fetch('AES_s2');
SELECT keyring_key_type_fetch('AES_s3');

--echo # keyring key_length_fetch tests
SELECT keyring_key_length_fetch('AES_g1');
SELECT keyring_key_length_fetch('AES_g2');
SELECT keyring_key_length_fetch('AES_g3');
SELECT keyring_key_length_fetch('AES_s1');
SELECT keyring_key_length_fetch('AES_s2');
SELECT keyring_key_length_fetch('AES_s3');

--echo # Restarting the server
--source include/force_restart.inc

--echo # keyring_key_fetch tests after restart
SELECT keyring_key_fetch('AES_g1') INTO @aes_g1_fetched_after_restart;
SELECT keyring_key_fetch('AES_g2') INTO @aes_g2_fetched_after_restart;
SELECT keyring_key_fetch('AES_g3') INTO @aes_g3_fetched_after_restart;
SELECT LENGTH(@aes_g1_fetched_after_restart);
SELECT LENGTH(@aes_g2_fetched_after_restart);
SELECT LENGTH(@aes_g3_fetched_after_restart);

SELECT keyring_key_fetch('AES_s1');
SELECT keyring_key_fetch('AES_s2');
SELECT keyring_key_fetch('AES_s3');

--echo # keyring key_type_fetch tests after restart
SELECT keyring_key_type_fetch('AES_g1');
SELECT keyring_key_type_fetch('AES_g2');
SELECT keyring_key_type_fetch('AES_g3');

--echo # keyring key_length_fetch tests after restart
SELECT keyring_key_length_fetch('AES_g1');
SELECT keyring_key_length_fetch('AES_g2');
SELECT keyring_key_length_fetch('AES_g3');

--echo # keyring_key_remove tests
SELECT keyring_key_remove('AES_g1');
SELECT keyring_key_remove('AES_g2');
SELECT keyring_key_remove('AES_g3');
SELECT keyring_key_remove('AES_s1');
SELECT keyring_key_remove('AES_s2');
SELECT keyring_key_remove('AES_s3');

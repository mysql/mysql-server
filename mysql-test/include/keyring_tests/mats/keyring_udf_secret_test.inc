
--echo # keyring_key_generate tests
SELECT keyring_key_generate('SECRET_g1', 'SECRET', 16);
SELECT keyring_key_generate('SECRET_g2', 'SECRET', 32);
SELECT keyring_key_generate('SECRET_g3', 'SECRET', 64);

--echo # keyring_key_store tests
SELECT keyring_key_store('SECRET_s1', 'SECRET', 'Quick brown fox jumped over the lazy dog');
SELECT keyring_key_store('SECRET_s2', 'SECRET', 'Old MacDonald had a farm');
SELECT keyring_key_store('SECRET_s3', 'SECRET', 'Quis custodiet ipsos custodes');

--echo # keyring_key_fetch tests
SELECT keyring_key_fetch('SECRET_g1') INTO @secret_g1_fetched;
SELECT keyring_key_fetch('SECRET_g2') INTO @secret_g2_fetched;
SELECT keyring_key_fetch('SECRET_g3') INTO @secret_g3_fetched;
SELECT LENGTH(@secret_g1_fetched);
SELECT LENGTH(@secret_g2_fetched);
SELECT LENGTH(@secret_g3_fetched);

SELECT keyring_key_fetch('SECRET_s1');
SELECT keyring_key_fetch('SECRET_s2');
SELECT keyring_key_fetch('SECRET_s3');

--echo # keyring key_type_fetch tests
SELECT keyring_key_type_fetch('SECRET_g1');
SELECT keyring_key_type_fetch('SECRET_g2');
SELECT keyring_key_type_fetch('SECRET_g3');
SELECT keyring_key_type_fetch('SECRET_s1');
SELECT keyring_key_type_fetch('SECRET_s2');
SELECT keyring_key_type_fetch('SECRET_s3');

--echo # keyring key_length_fetch tests
SELECT keyring_key_length_fetch('SECRET_g1');
SELECT keyring_key_length_fetch('SECRET_g2');
SELECT keyring_key_length_fetch('SECRET_g3');
SELECT keyring_key_length_fetch('SECRET_s1');
SELECT keyring_key_length_fetch('SECRET_s2');
SELECT keyring_key_length_fetch('SECRET_s3');

--echo # Restarting the server
--source include/force_restart.inc

--echo # keyring_key_fetch tests after restart
SELECT keyring_key_fetch('SECRET_g1') INTO @secret_g1_fetched_after_restart;
SELECT keyring_key_fetch('SECRET_g2') INTO @secret_g2_fetched_after_restart;
SELECT keyring_key_fetch('SECRET_g3') INTO @secret_g3_fetched_after_restart;
SELECT LENGTH(@secret_g1_fetched_after_restart);
SELECT LENGTH(@secret_g2_fetched_after_restart);
SELECT LENGTH(@secret_g3_fetched_after_restart);

SELECT keyring_key_fetch('SECRET_s1');
SELECT keyring_key_fetch('SECRET_s2');
SELECT keyring_key_fetch('SECRET_s3');

--echo # keyring key_type_fetch tests after restart
SELECT keyring_key_type_fetch('SECRET_g1');
SELECT keyring_key_type_fetch('SECRET_g2');
SELECT keyring_key_type_fetch('SECRET_g3');

--echo # keyring key_length_fetch tests after restart
SELECT keyring_key_length_fetch('SECRET_g1');
SELECT keyring_key_length_fetch('SECRET_g2');
SELECT keyring_key_length_fetch('SECRET_g3');

--echo # keyring_key_remove tests
SELECT keyring_key_remove('SECRET_g1');
SELECT keyring_key_remove('SECRET_g2');
SELECT keyring_key_remove('SECRET_g3');
SELECT keyring_key_remove('SECRET_s1');
SELECT keyring_key_remove('SECRET_s2');
SELECT keyring_key_remove('SECRET_s3');

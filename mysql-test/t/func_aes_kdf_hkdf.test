--source include/have_tlsv13.inc

--echo # Tests of the AES KDF hkdf functionality

--echo #### AES_ENCRYPT return type
--echo # must work and return a string
SELECT TO_BASE64(AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf'));
--echo # must return 16
SELECT LENGTH(AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf'));
--echo # must return binary
SELECT CHARSET(AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf'));
--echo # must be equal
SELECT AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf') = AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf');

--echo # Tests of AES strong key generation
--echo # Strong key generation with KDF, should not be equal keys
SELECT AES_ENCRYPT('my_text', repeat("x",32), '', 'hkdf') = AES_ENCRYPT('my_text', repeat("y",32), '', 'hkdf');

--echo # Strong key generation with KDF, should not be equal keys
SELECT AES_ENCRYPT('my_text', repeat("x",32), '', 'hkdf') = AES_ENCRYPT('my_text', '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0', '', 'hkdf');

--echo #### AES_ENCRYPT KDF hkdf parameters
select TO_BASE64(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf'));
select TO_BASE64(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf', 'salt'));
select TO_BASE64(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf', 'salt', 'info'));
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf'), 'my_key_string', '', 'hkdf');

--echo #### AES_ENCRYPT KDF hkdf parameters with incorrect data types
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf', 10001), 'my_key_string', '', 'hkdf', 10001);
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf', 10001, 2000), 'my_key_string', '', 'hkdf', 10001, 2000);

--echo # KDF function name different case.
--error ER_AES_INVALID_KDF_NAME
select aes_encrypt("foo",repeat("x",16),NULL,'hKdF');

--echo #### AES_ENCRYPT KDF hkdf parameters with initialization vector
SET @IV=REPEAT('a', 16);
--echo #### aes-128-cbc
SELECT @@session.block_encryption_mode INTO @save_block_encryption_mode;
eval SET SESSION block_encryption_mode="aes-128-cbc";
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', @IV, 'hkdf'), 'my_key_string', @IV, 'hkdf');
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', @IV, 'hkdf', 'salt'), 'my_key_string', @IV, 'hkdf', 'salt');
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', @IV, 'hkdf', 'salt', 'info'), 'my_key_string', @IV, 'hkdf', 'salt', 'info');
SET SESSION block_encryption_mode=@save_block_encryption_mode;
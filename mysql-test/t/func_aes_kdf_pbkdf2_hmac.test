--echo # Tests of the AES KDF functionality

--echo #### AES_ENCRYPT return type
--echo # must work and return a string
SELECT TO_BASE64(AES_ENCRYPT('my_text', 'my_key_string', '', 'pbkdf2_hmac'));
--echo # must return 16
SELECT LENGTH(AES_ENCRYPT('my_text', 'my_key_string', '', 'pbkdf2_hmac'));
--echo # must return binary
SELECT CHARSET(AES_ENCRYPT('my_text', 'my_key_string', '', 'pbkdf2_hmac'));
--echo # must be equal
SELECT AES_ENCRYPT('my_text', 'my_key_string', '', 'pbkdf2_hmac') = AES_ENCRYPT('my_text', 'my_key_string', '', 'pbkdf2_hmac');

--echo #### AES_ENCRYPT KDF pbkdf2_hmac parameters
select TO_BASE64(AES_ENCRYPT('my_text','my_key_string', '', 'pbkdf2_hmac'));
select TO_BASE64(AES_ENCRYPT('my_text','my_key_string', '', 'pbkdf2_hmac', 'salt'));
select TO_BASE64(AES_ENCRYPT('my_text','my_key_string', '', 'pbkdf2_hmac', 'salt', '10001'));
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', '', 'pbkdf2_hmac'), 'my_key_string', '', 'pbkdf2_hmac');

--echo # Tests of AES strong key generation

--echo # Weak key generation without KDF, should be equal output
SELECT AES_ENCRYPT('my_text', repeat("x",32), '') = AES_ENCRYPT('my_text', repeat("y",32), '');

--echo # Strong key generation with KDF, should not be equal output
SELECT AES_ENCRYPT('my_text', repeat("x",32), '', 'pbkdf2_hmac') = AES_ENCRYPT('my_text', repeat("y",32), '', 'pbkdf2_hmac');

--echo # Strong key generation with KDF, should not be equal output
SELECT AES_ENCRYPT('my_text', repeat("x",32), '', 'pbkdf2_hmac') = AES_ENCRYPT('my_text', '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0', '', 'pbkdf2_hmac');

--echo #### AES_ENCRYPT KDF pbkdf2_hmac parameters with incorrect data types
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', '', 'pbkdf2_hmac', 4000, '10001'), 'my_key_string', '', 'pbkdf2_hmac',4000, '10001');
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', '', 'pbkdf2_hmac', 4000, 10001), 'my_key_string', '', 'pbkdf2_hmac',4000, 10001);

--echo #### AES_ENCRYPT KDF error conditions

--echo # Invalid KDF method
--error ER_AES_INVALID_KDF_NAME
select AES_ENCRYPT('my_text','my_key_string', '', 'invalid');

--echo # Warning for big AES key and empty KDF method
select AES_ENCRYPT('my_text', repeat("x",32));

--echo # No warning for smaller key
select AES_ENCRYPT('my_text', 'my_key');

--echo # KDF pbkdf2_hmac iterations less then 1000 error.
--error ER_AES_INVALID_KDF_ITERATIONS
select AES_ENCRYPT('my_text','my_key_string', '', 'pbkdf2_hmac', 'salt', '100');

--echo # KDF pbkdf2_hmac iterations as text
--error ER_AES_INVALID_KDF_ITERATIONS
select AES_ENCRYPT('my_text','my_key_string', '', 'pbkdf2_hmac', 'salt', 'aa');

--echo # KDF function name very large.
--error ER_AES_INVALID_KDF_OPTION_SIZE
select aes_encrypt("foo",repeat("x",16),NULL,repeat("1",10000000000));

--echo # KDF function name large
--error ER_AES_INVALID_KDF_OPTION_SIZE
select aes_encrypt("foo",repeat("x",16),NULL,repeat("1",300));

--echo # KDF function name different case.
--error ER_AES_INVALID_KDF_NAME
select aes_encrypt("foo",repeat("x",16),NULL,'pbkdf2_HMac');

--echo # Extra IV
--error ER_AES_INVALID_KDF_NAME
select aes_encrypt("foo",repeat("x",16),NULL,'pbkdf2_HMac');

--echo #### AES_ENCRYPT KDF pbkdf2_hmac parameters with initialization vector
SET @IV=REPEAT('a', 16);
--echo #### aes-128-cbc
SELECT @@session.block_encryption_mode INTO @save_block_encryption_mode;
eval SET SESSION block_encryption_mode="aes-128-cbc";
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', @IV, 'pbkdf2_hmac'), 'my_key_string', @IV, 'pbkdf2_hmac');
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', @IV, 'pbkdf2_hmac', 'salt'), 'my_key_string', @IV, 'pbkdf2_hmac', 'salt');
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', @IV, 'pbkdf2_hmac', 'salt', '10001'), 'my_key_string', @IV, 'pbkdf2_hmac', 'salt', '10001');
SET SESSION block_encryption_mode=@save_block_encryption_mode;

# Tests of the AES KDF hkdf functionality
#### AES_ENCRYPT return type
# must work and return a string
SELECT TO_BASE64(AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf'));
TO_BASE64(AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf'))
9UwMLA5qdPChE0NbqnTYGw==
# must return 16
SELECT LENGTH(AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf'));
LENGTH(AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf'))
16
# must return binary
SELECT CHARSET(AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf'));
CHARSET(AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf'))
binary
# must be equal
SELECT AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf') = AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf');
AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf') = AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf')
1
# Tests of AES strong key generation
# Strong key generation with KDF, should not be equal keys
SELECT AES_ENCRYPT('my_text', repeat("x",32), '', 'hkdf') = AES_ENCRYPT('my_text', repeat("y",32), '', 'hkdf');
AES_ENCRYPT('my_text', repeat("x",32), '', 'hkdf') = AES_ENCRYPT('my_text', repeat("y",32), '', 'hkdf')
0
# Strong key generation with KDF, should not be equal keys
SELECT AES_ENCRYPT('my_text', repeat("x",32), '', 'hkdf') = AES_ENCRYPT('my_text', '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0', '', 'hkdf');
AES_ENCRYPT('my_text', repeat("x",32), '', 'hkdf') = AES_ENCRYPT('my_text', '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0', '', 'hkdf')
0
#### AES_ENCRYPT KDF hkdf parameters
select TO_BASE64(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf'));
TO_BASE64(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf'))
9UwMLA5qdPChE0NbqnTYGw==
select TO_BASE64(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf', 'salt'));
TO_BASE64(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf', 'salt'))
nmOEqhC+wjUalOcakrMOQw==
select TO_BASE64(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf', 'salt', 'info'));
TO_BASE64(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf', 'salt', 'info'))
QzgVeH3yZT7XefHW7G36nQ==
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf'), 'my_key_string', '', 'hkdf');
'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', '', 'hkdf'), 'my_key_string', '', 'hkdf')
1
#### AES_ENCRYPT KDF hkdf parameters with incorrect data types
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf', 10001), 'my_key_string', '', 'hkdf', 10001);
'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf', 10001), 'my_key_string', '', 'hkdf', 10001)
1
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf', 10001, 2000), 'my_key_string', '', 'hkdf', 10001, 2000);
'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text','my_key_string', '', 'hkdf', 10001, 2000), 'my_key_string', '', 'hkdf', 10001, 2000)
1
# KDF function name different case.
select aes_encrypt("foo",repeat("x",16),NULL,'hKdF');
ERROR HY000: KDF method name is not valid. Please use hkdf or pbkdf2_hmac method name
#### AES_ENCRYPT KDF hkdf parameters with initialization vector
SET @IV=REPEAT('a', 16);
#### aes-128-cbc
SELECT @@session.block_encryption_mode INTO @save_block_encryption_mode;
SET SESSION block_encryption_mode="aes-128-cbc";
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', @IV, 'hkdf'), 'my_key_string', @IV, 'hkdf');
'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', @IV, 'hkdf'), 'my_key_string', @IV, 'hkdf')
1
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', @IV, 'hkdf', 'salt'), 'my_key_string', @IV, 'hkdf', 'salt');
'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', @IV, 'hkdf', 'salt'), 'my_key_string', @IV, 'hkdf', 'salt')
1
SELECT 'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', @IV, 'hkdf', 'salt', 'info'), 'my_key_string', @IV, 'hkdf', 'salt', 'info');
'my_text' = AES_DECRYPT(AES_ENCRYPT('my_text', 'my_key_string', @IV, 'hkdf', 'salt', 'info'), 'my_key_string', @IV, 'hkdf', 'salt', 'info')
1
SET SESSION block_encryption_mode=@save_block_encryption_mode;

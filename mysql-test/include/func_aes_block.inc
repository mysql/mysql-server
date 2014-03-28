#
# Parameters:
#
# $block_mode


--echo #### $block_mode

SET @IVA=REPEAT('a', 16);
SET @IVB=REPEAT('b', 16);
SET @KEY1=REPEAT('c', 16); 
SET @KEY2=REPEAT('d', 16); 


--echo #### 128-$block_mode

eval SET SESSION block_encryption_mode="aes-128-$block_mode";

--echo # must throw an error without an IV
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT AES_ENCRYPT('a', @KEY1);

--echo block mode dependent. Must be non-0 and non-null
SELECT LENGTH(AES_ENCRYPT('a', @KEY1, @IVA));

--echo block mode dependent
SELECT TO_BASE64(AES_ENCRYPT('a', @KEY1, @IVA));

--echo # must be equal
SELECT 'a' = AES_DECRYPT(AES_ENCRYPT('a', @KEY1, @IVA), @KEY1, @IVA);

--echo # must not be equal
SELECT 'a' = AES_DECRYPT(AES_ENCRYPT('a',@KEY1, @IVA), @KEY1, @IVB);
SELECT 'a' = AES_DECRYPT(AES_ENCRYPT('a',@KEY1, @IVA), @KEY2, @IVA);
SELECT 'a' = AES_DECRYPT(AES_ENCRYPT('b',@KEY1, @IVA), @KEY1, @IVA);


--echo #### 192-$block_mode

eval SET SESSION block_encryption_mode="aes-192-$block_mode";

--echo # must throw an error without an IV
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT AES_ENCRYPT('a', @KEY1);

--echo # must be equal
SELECT 'a' = AES_DECRYPT(AES_ENCRYPT('a',@KEY1, @IVA), @KEY1, @IVA);

--echo # must not be equal
SELECT 'a' = AES_DECRYPT(AES_ENCRYPT('a',@KEY1, @IVA), @KEY1, @IVB);
SELECT 'a' = AES_DECRYPT(AES_ENCRYPT('a',@KEY1, @IVA), @KEY2, @IVA);
SELECT 'a' = AES_DECRYPT(AES_ENCRYPT('b',@KEY1, @IVA), @KEY1, @IVA);


--echo #### 256-$block_mode

eval SET SESSION block_encryption_mode="aes-256-$block_mode";

--echo # must throw an error without an IV
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT AES_ENCRYPT('a', @KEY1);

--echo # must be equal
SELECT 'a' = AES_DECRYPT(AES_ENCRYPT('a',@KEY1, @IVA), @KEY1, @IVA);

--echo # must not be equal
SELECT 'a' = AES_DECRYPT(AES_ENCRYPT('a',@KEY1, @IVA), @KEY1, @IVB);
SELECT 'a' = AES_DECRYPT(AES_ENCRYPT('a',@KEY1, @IVA), @KEY2, @IVA);
SELECT 'a' = AES_DECRYPT(AES_ENCRYPT('b',@KEY1, @IVA), @KEY1, @IVA);

SET SESSION block_encryption_mode=DEFAULT;


--echo #### 128, 192 and 256 bit $block_mode


eval CREATE TABLE aes_$block_mode(a VARCHAR(128), b128 VARCHAR(144),
  b192 VARCHAR(144), b256 CHAR(144));
eval INSERT INTO aes_$block_mode (a) VALUES (REPEAT('a', 128));
eval INSERT INTO aes_$block_mode (a) VALUES (REPEAT(0x00313233, 32));

eval SET SESSION block_encryption_mode="aes-128-$block_mode";
eval UPDATE aes_$block_mode SET b128 = AES_ENCRYPT(a, @KEY1, @IVA);

eval SET SESSION block_encryption_mode="aes-192-$block_mode";
eval UPDATE aes_$block_mode SET b192 = AES_ENCRYPT(a, @KEY1, @IVA);

eval SET SESSION block_encryption_mode="aes-256-$block_mode";
eval UPDATE aes_$block_mode SET b256 = AES_ENCRYPT(a, @KEY1, @IVA);

--echo # must return 0
eval SELECT COUNT(*) FROM aes_$block_mode WHERE b128 = b192 OR B192 = b256 OR b128=b256;

eval SET SESSION block_encryption_mode="aes-256-$block_mode";

--echo # must return 2
eval SELECT COUNT(*) FROM aes_$block_mode WHERE a = AES_DECRYPT(b256, @KEY1, @IVA);

--echo # must return 0
eval SELECT COUNT(*) FROM aes_$block_mode WHERE a = AES_DECRYPT(b256, 'b', @IVA);

--echo # must return 0
eval SELECT COUNT(*) FROM aes_$block_mode WHERE a = AES_DECRYPT(b256, @KEY1, @IVB);

eval SET SESSION block_encryption_mode="aes-192-$block_mode";

--echo # must return 2
eval SELECT COUNT(*) FROM aes_$block_mode WHERE a = AES_DECRYPT(b192, @KEY1, @IVA);

--echo # must return 0
eval SELECT COUNT(*) FROM aes_$block_mode WHERE a = AES_DECRYPT(b192, @KEY2, @IVA);

--echo # must return 0
eval SELECT COUNT(*) FROM aes_$block_mode WHERE a = AES_DECRYPT(b256, @KEY1, @IVB);

eval SET SESSION block_encryption_mode="aes-128-$block_mode";

--echo # must return 2
eval SELECT COUNT(*) FROM aes_$block_mode WHERE a = AES_DECRYPT(b128, @KEY1, @IVA);

--echo # must return 0
eval SELECT COUNT(*) FROM aes_$block_mode WHERE a = AES_DECRYPT(b128, @KEY2, @IVA);

--echo # must return 0
eval SELECT COUNT(*) FROM aes_$block_mode WHERE a = AES_DECRYPT(b256, @KEY2, @IVB);


SET SESSION block_encryption_mode=DEFAULT;
eval DROP TABLE aes_$block_mode;


SELECT keyring_key_type_fetch('secret_test_keystore');
SELECT keyring_key_length_fetch('secret_test_keystore');
SELECT keyring_key_fetch('secret_test_keystore');
SELECT c1 FROM t1 WHERE id = 1 INTO @cert_value_table;
SELECT keyring_key_type_fetch('secret_test_pemfile');
SELECT keyring_key_length_fetch('secret_test_pemfile');
SELECT keyring_key_fetch('secret_test_pemfile') INTO @cert_value_keyring;
SELECT @cert_value_table = @cert_value_keyring;
SET @length = 16384;
if ($length_restrictions)
{
  --echo Restricting the length
  SET @length = 4096;
}
SELECT REPEAT('a',@length) INTO @long_store;
SELECT keyring_key_fetch('secret_test_longstore') INTO @long_store_retrieved;
SELECT @long_store = @long_store_retrieved;

SELECT keyring_key_fetch('secret_test_longgenerated') INTO @long_generated_retrieved;
SELECT keyring_key_length_fetch('secret_test_longgenerated');

SELECT KEY_ID, KEY_OWNER FROM performance_schema.keyring_keys WHERE
KEY_ID LIKE 'secret_test%' ORDER BY KEY_ID, KEY_OWNER;
SELECT keyring_key_remove('secret_test_keystore');
SELECT keyring_key_remove('secret_test_keygenerate');
SELECT keyring_key_remove('secret_test_pemfile');
SELECT keyring_key_remove('secret_test_longstore');
SELECT keyring_key_remove('secret_test_longgenerated');
DROP TABLE t1;

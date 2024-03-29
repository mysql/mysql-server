# If new keys are added in future, prefix it with secret_test so that
# test can retrieve and check the validity of the key.

SELECT keyring_key_store('secret_test_keystore', 'SECRET', 'abcdefgh');
SELECT keyring_key_type_fetch('secret_test_keystore');
SELECT keyring_key_length_fetch('secret_test_keystore');
SELECT keyring_key_fetch('secret_test_keystore');

SELECT keyring_key_generate('secret_test_keygenerate', 'SECRET', 8);
SELECT keyring_key_type_fetch('secret_test_keygenerate');
SELECT keyring_key_length_fetch('secret_test_keygenerate');
SELECT keyring_key_fetch('secret_test_keystore') into @keystore_retrieved;
select LENGTH(@keystore_retrieved);

CREATE TABLE t1(id INT AUTO_INCREMENT, c1 TEXT, PRIMARY KEY(id));
LOAD DATA INFILE '../../std_data/secret_data_cert.txt' INTO TABLE t1 LINES TERMINATED BY 'hahaha' (c1);
SELECT c1 FROM t1 WHERE id = 1 INTO @cert_value_table;
SELECT keyring_key_store('secret_test_pemfile', 'SECRET', @cert_value_table);
SELECT keyring_key_type_fetch('secret_test_pemfile');
SELECT keyring_key_length_fetch('secret_test_pemfile');
SELECT keyring_key_fetch('secret_test_pemfile') INTO @cert_value_keyring;
SELECT @cert_value_table = @cert_value_keyring;

SET @store_length = 16384;
SET @generate_length = @store_length;
if ($length_restrictions)
{
  --echo Restricting the length
  SET @store_length = 4096;
  SET @generate_length = 1024;
}
SELECT REPEAT('a',@store_length) INTO @long_store;
SELECT keyring_key_store('secret_test_longstore', 'SECRET', @long_store);
SELECT keyring_key_fetch('secret_test_longstore') INTO @long_store_retrieved;
SELECT @long_store = @long_store_retrieved;

SELECT keyring_key_generate('secret_test_longgenerated', 'SECRET', @generate_length);
SELECT keyring_key_fetch('secret_test_longgenerated') INTO @long_generated_retrieved;
SELECT keyring_key_length_fetch('secret_test_longgenerated');
SELECT keyring_key_fetch('secret_test_longstore') INTO @secret_test_longstore_retrieved;
select LENGTH(@secret_test_longstore_retrieved);

SELECT KEY_ID, KEY_OWNER FROM performance_schema.keyring_keys WHERE
KEY_ID LIKE 'secret_test%' ORDER BY KEY_ID, KEY_OWNER;



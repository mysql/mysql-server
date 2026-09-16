
--echo #============= preparation =============#
--source include/keyring_tests/helper/install_keyring_udf.inc

CREATE DATABASE tmp;
USE tmp;
CREATE TABLE name(id INT, name varchar(10));
INSERT INTO name VALUES (1, '');
INSERT INTO name VALUES (2, NULL);
INSERT INTO name VALUES (3, 'key3');
CREATE TABLE len(id INT, name varchar(10), len INT);
INSERT INTO len VALUES (1, 'key1', -1);
INSERT INTO len VALUES (2, 'key2', NULL);
INSERT INTO len VALUES (3, 'key3', 20000);
INSERT INTO len VALUES (4, 'key4', 16);
CREATE TABLE type(id INT, name varchar(10), type varchar(10));
INSERT INTO type VALUES (1, 'key1', '');
INSERT INTO type VALUES (2, 'key2', NULL);
INSERT INTO type VALUES (3, 'key3', 'invalid');
INSERT INTO type VALUES (4, 'key4', 'SECRET');
CREATE TABLE val(id INT, name varchar(10), val varchar(16));
INSERT INTO val VALUES (1, 'key1', '');
INSERT INTO val VALUES (2, 'key2', NULL);
INSERT INTO val VALUES (3, 'key3', 'Valid key text');



--echo #=========== keyring_key_generate ===========#
# NULL key name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_generate(NULL, 'SECRET', 16);
# empty key name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_generate('', 'SECRET', 16);

--echo # The last row should be 1, the rest should be NULL
SELECT keyring_key_generate(name, 'SECRET', 16) FROM name ORDER BY id;
# cleanup
SELECT keyring_key_remove('key3');

# NULL type name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_generate('key', NULL, 16);
# typed NULL type name
SELECT keyring_key_generate('key', CAST(NULL AS CHAR), 16);
# empty type name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_generate('key', '', 16);
# invalid type
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_generate('key', 'invalid-type', 32);

--echo # For a keyring component: the last 2 rows should be 1, the rest should be NULL
--echo # For a keyring plugin: the last row should be 1, the rest should be NULL
SELECT keyring_key_generate(name, type, 16) FROM type ORDER BY id;
# cleanup
SELECT keyring_key_remove('key4');

# NULL length
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_generate('key', 'SECRET', NULL);
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_generate('key', 'SECRET', CAST(NULL AS CHAR));
SELECT keyring_key_generate('key', 'SECRET', CAST(NULL AS SIGNED));
# negative length
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_generate('key', 'SECRET', -1);
# 0 length
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_generate('key', 'SECRET', 0);
# too long
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_generate('key', 'SECRET', 18000);

--echo # The last row should be 1, the rest should be NULL
SELECT keyring_key_generate(name, 'SECRET', len) FROM len ORDER BY id;
# cleanup
SELECT keyring_key_remove('key4');

--echo #=========== keyring_key_store ===========#
# NULL key name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_store(NULL, 'SECRET', 'Valid key text');
# empty key name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_store('', 'SECRET', 'Valid key text');

--echo # The last row should be 1, the rest should be NULL
SELECT keyring_key_store(name, 'SECRET', 'Valid key text') FROM name ORDER BY id;
# cleanup
SELECT keyring_key_remove('key3');

# NULL type name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_store('key', NULL, 'Valid key text');
# empty type name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_store('key', '', 'Valid key text');
# invalid type
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_store('key', 'invalid-type', 'Valid key text');

--echo # For a keyring component: the last 2 rows should be 1, the rest should be NULL
--echo # For a keyring plugin: the last row should be 1, the rest should be NULL
SELECT keyring_key_store(name, type, 'Valid key text') FROM type ORDER BY id;
# cleanup
SELECT keyring_key_remove('key4');

# NULL value
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_store('key', 'SECRET', NULL);
# empty value
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_store('key', 'SECRET', '');

--echo # The last row should be 1, the rest should be NULL
SELECT keyring_key_store(name, 'SECRET', val) FROM val ORDER BY id;
# cleanup
SELECT keyring_key_remove('key3');

--echo #=========== keyring_key_fetch ===========#
# NULL key name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_fetch(NULL);
# empty key name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_fetch('');
# key not exists
let $is_null= `SELECT ISNULL(keyring_key_fetch('nonexistent'))`;
if (!$is_null) {
	die Result should be NULL;
}

--echo # all should be NULL
SELECT keyring_key_fetch(name) FROM name ORDER BY id;

--echo #=========== keyring_key_type_fetch ===========#
# NULL key name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_type_fetch(NULL);
# empty key name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_type_fetch('');
# key not exists
let $is_null= `SELECT ISNULL(keyring_key_type_fetch('nonexistent'))`;
if (!$is_null) {
	die Result should be NULL;
}

--echo # all should be NULL
SELECT keyring_key_type_fetch(name) FROM name ORDER BY id;

--echo #=========== keyring_key_length_fetch ===========#
# NULL key name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_length_fetch(NULL);
# empty key name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_length_fetch('');
# key not exists
let $is_null= `SELECT ISNULL(keyring_key_length_fetch('nonexistent'))`;
if (!$is_null) {
	die Result should be NULL;
}

--echo # all should be NULL
SELECT keyring_key_length_fetch(name) FROM name ORDER BY id;

--echo #=========== keyring_key_remove ===========#
# NULL key name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_remove(NULL);
# empty key name
--error ER_CANT_INITIALIZE_UDF
SELECT keyring_key_remove('');
# key not exists
let $is_null= `SELECT ISNULL(keyring_key_remove('nonexistent'))`;
if (!$is_null) {
	die Result should be NULL;
}

--echo # all should be NULL
SELECT keyring_key_remove(name) FROM name ORDER BY id;

--echo #=============== cleanup ===============#
--source include/keyring_tests/helper/uninstall_keyring_udf.inc

DROP DATABASE tmp;

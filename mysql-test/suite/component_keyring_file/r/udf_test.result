# ----------------------------------------------------------------------
# Setup
# Creating local configuration file for keyring component: component_keyring_file
# Creating manifest file for current MySQL server instance
# Re-starting mysql server with manifest file
# ----------------------------------------------------------------------
# ----------------------------------------------------------------------
# Setting up Keyring UDFs
INSTALL PLUGIN keyring_udf SONAME 'keyring_udf.so';
CREATE FUNCTION keyring_key_store RETURNS INTEGER SONAME 'keyring_udf.so';
CREATE FUNCTION keyring_key_generate RETURNS INTEGER SONAME 'keyring_udf.so';
CREATE FUNCTION keyring_key_fetch RETURNS sTRING SONAME 'keyring_udf.so';
CREATE FUNCTION keyring_key_type_fetch RETURNS STRING SONAME 'keyring_udf.so';
CREATE FUNCTION keyring_key_length_fetch RETURNS INTEGER SONAME 'keyring_udf.so';
CREATE FUNCTION keyring_key_remove RETURNS INTEGER SONAME 'keyring_udf.so';
# ----------------------------------------------------------------------
# Tests for AES key type
# keyring_key_generate tests
SELECT keyring_key_generate('AES_g1', 'AES', 16);
keyring_key_generate('AES_g1', 'AES', 16)
1
SELECT keyring_key_generate('AES_g2', 'AES', 32);
keyring_key_generate('AES_g2', 'AES', 32)
1
SELECT keyring_key_generate('AES_g3', 'AES', 64);
keyring_key_generate('AES_g3', 'AES', 64)
1
# keyring_key_store tests
SELECT keyring_key_store('AES_s1', 'AES', 'Quick brown fox jumped over the lazy dog');
keyring_key_store('AES_s1', 'AES', 'Quick brown fox jumped over the lazy dog')
1
SELECT keyring_key_store('AES_s2', 'AES', 'Old MacDonald had a farm');
keyring_key_store('AES_s2', 'AES', 'Old MacDonald had a farm')
1
SELECT keyring_key_store('AES_s3', 'AES', 'Quis custodiet ipsos custodes');
keyring_key_store('AES_s3', 'AES', 'Quis custodiet ipsos custodes')
1
# keyring_key_fetch tests
SELECT keyring_key_fetch('AES_g1') INTO @aes_g1_fetched;
SELECT keyring_key_fetch('AES_g2') INTO @aes_g2_fetched;
SELECT keyring_key_fetch('AES_g3') INTO @aes_g3_fetched;
SELECT LENGTH(@aes_g1_fetched);
LENGTH(@aes_g1_fetched)
16
SELECT LENGTH(@aes_g2_fetched);
LENGTH(@aes_g2_fetched)
32
SELECT LENGTH(@aes_g3_fetched);
LENGTH(@aes_g3_fetched)
64
SELECT keyring_key_fetch('AES_s1');
keyring_key_fetch('AES_s1')
Quick brown fox jumped over the lazy dog
SELECT keyring_key_fetch('AES_s2');
keyring_key_fetch('AES_s2')
Old MacDonald had a farm
SELECT keyring_key_fetch('AES_s3');
keyring_key_fetch('AES_s3')
Quis custodiet ipsos custodes
# keyring key_type_fetch tests
SELECT keyring_key_type_fetch('AES_g1');
keyring_key_type_fetch('AES_g1')
AES
SELECT keyring_key_type_fetch('AES_g2');
keyring_key_type_fetch('AES_g2')
AES
SELECT keyring_key_type_fetch('AES_g3');
keyring_key_type_fetch('AES_g3')
AES
SELECT keyring_key_type_fetch('AES_s1');
keyring_key_type_fetch('AES_s1')
AES
SELECT keyring_key_type_fetch('AES_s2');
keyring_key_type_fetch('AES_s2')
AES
SELECT keyring_key_type_fetch('AES_s3');
keyring_key_type_fetch('AES_s3')
AES
# keyring key_length_fetch tests
SELECT keyring_key_length_fetch('AES_g1');
keyring_key_length_fetch('AES_g1')
16
SELECT keyring_key_length_fetch('AES_g2');
keyring_key_length_fetch('AES_g2')
32
SELECT keyring_key_length_fetch('AES_g3');
keyring_key_length_fetch('AES_g3')
64
SELECT keyring_key_length_fetch('AES_s1');
keyring_key_length_fetch('AES_s1')
40
SELECT keyring_key_length_fetch('AES_s2');
keyring_key_length_fetch('AES_s2')
24
SELECT keyring_key_length_fetch('AES_s3');
keyring_key_length_fetch('AES_s3')
29
# Restarting the server
# keyring_key_fetch tests after restart
SELECT keyring_key_fetch('AES_g1') INTO @aes_g1_fetched_after_restart;
SELECT keyring_key_fetch('AES_g2') INTO @aes_g2_fetched_after_restart;
SELECT keyring_key_fetch('AES_g3') INTO @aes_g3_fetched_after_restart;
SELECT LENGTH(@aes_g1_fetched_after_restart);
LENGTH(@aes_g1_fetched_after_restart)
16
SELECT LENGTH(@aes_g2_fetched_after_restart);
LENGTH(@aes_g2_fetched_after_restart)
32
SELECT LENGTH(@aes_g3_fetched_after_restart);
LENGTH(@aes_g3_fetched_after_restart)
64
SELECT keyring_key_fetch('AES_s1');
keyring_key_fetch('AES_s1')
Quick brown fox jumped over the lazy dog
SELECT keyring_key_fetch('AES_s2');
keyring_key_fetch('AES_s2')
Old MacDonald had a farm
SELECT keyring_key_fetch('AES_s3');
keyring_key_fetch('AES_s3')
Quis custodiet ipsos custodes
# keyring key_type_fetch tests after restart
SELECT keyring_key_type_fetch('AES_g1');
keyring_key_type_fetch('AES_g1')
AES
SELECT keyring_key_type_fetch('AES_g2');
keyring_key_type_fetch('AES_g2')
AES
SELECT keyring_key_type_fetch('AES_g3');
keyring_key_type_fetch('AES_g3')
AES
# keyring key_length_fetch tests after restart
SELECT keyring_key_length_fetch('AES_g1');
keyring_key_length_fetch('AES_g1')
16
SELECT keyring_key_length_fetch('AES_g2');
keyring_key_length_fetch('AES_g2')
32
SELECT keyring_key_length_fetch('AES_g3');
keyring_key_length_fetch('AES_g3')
64
# keyring_key_remove tests
SELECT keyring_key_remove('AES_g1');
keyring_key_remove('AES_g1')
1
SELECT keyring_key_remove('AES_g2');
keyring_key_remove('AES_g2')
1
SELECT keyring_key_remove('AES_g3');
keyring_key_remove('AES_g3')
1
SELECT keyring_key_remove('AES_s1');
keyring_key_remove('AES_s1')
1
SELECT keyring_key_remove('AES_s2');
keyring_key_remove('AES_s2')
1
SELECT keyring_key_remove('AES_s3');
keyring_key_remove('AES_s3')
1
# Tests for SECRET key type
# keyring_key_generate tests
SELECT keyring_key_generate('SECRET_g1', 'SECRET', 16);
keyring_key_generate('SECRET_g1', 'SECRET', 16)
1
SELECT keyring_key_generate('SECRET_g2', 'SECRET', 32);
keyring_key_generate('SECRET_g2', 'SECRET', 32)
1
SELECT keyring_key_generate('SECRET_g3', 'SECRET', 64);
keyring_key_generate('SECRET_g3', 'SECRET', 64)
1
# keyring_key_store tests
SELECT keyring_key_store('SECRET_s1', 'SECRET', 'Quick brown fox jumped over the lazy dog');
keyring_key_store('SECRET_s1', 'SECRET', 'Quick brown fox jumped over the lazy dog')
1
SELECT keyring_key_store('SECRET_s2', 'SECRET', 'Old MacDonald had a farm');
keyring_key_store('SECRET_s2', 'SECRET', 'Old MacDonald had a farm')
1
SELECT keyring_key_store('SECRET_s3', 'SECRET', 'Quis custodiet ipsos custodes');
keyring_key_store('SECRET_s3', 'SECRET', 'Quis custodiet ipsos custodes')
1
# keyring_key_fetch tests
SELECT keyring_key_fetch('SECRET_g1') INTO @secret_g1_fetched;
SELECT keyring_key_fetch('SECRET_g2') INTO @secret_g2_fetched;
SELECT keyring_key_fetch('SECRET_g3') INTO @secret_g3_fetched;
SELECT LENGTH(@secret_g1_fetched);
LENGTH(@secret_g1_fetched)
16
SELECT LENGTH(@secret_g2_fetched);
LENGTH(@secret_g2_fetched)
32
SELECT LENGTH(@secret_g3_fetched);
LENGTH(@secret_g3_fetched)
64
SELECT keyring_key_fetch('SECRET_s1');
keyring_key_fetch('SECRET_s1')
Quick brown fox jumped over the lazy dog
SELECT keyring_key_fetch('SECRET_s2');
keyring_key_fetch('SECRET_s2')
Old MacDonald had a farm
SELECT keyring_key_fetch('SECRET_s3');
keyring_key_fetch('SECRET_s3')
Quis custodiet ipsos custodes
# keyring key_type_fetch tests
SELECT keyring_key_type_fetch('SECRET_g1');
keyring_key_type_fetch('SECRET_g1')
SECRET
SELECT keyring_key_type_fetch('SECRET_g2');
keyring_key_type_fetch('SECRET_g2')
SECRET
SELECT keyring_key_type_fetch('SECRET_g3');
keyring_key_type_fetch('SECRET_g3')
SECRET
SELECT keyring_key_type_fetch('SECRET_s1');
keyring_key_type_fetch('SECRET_s1')
SECRET
SELECT keyring_key_type_fetch('SECRET_s2');
keyring_key_type_fetch('SECRET_s2')
SECRET
SELECT keyring_key_type_fetch('SECRET_s3');
keyring_key_type_fetch('SECRET_s3')
SECRET
# keyring key_length_fetch tests
SELECT keyring_key_length_fetch('SECRET_g1');
keyring_key_length_fetch('SECRET_g1')
16
SELECT keyring_key_length_fetch('SECRET_g2');
keyring_key_length_fetch('SECRET_g2')
32
SELECT keyring_key_length_fetch('SECRET_g3');
keyring_key_length_fetch('SECRET_g3')
64
SELECT keyring_key_length_fetch('SECRET_s1');
keyring_key_length_fetch('SECRET_s1')
40
SELECT keyring_key_length_fetch('SECRET_s2');
keyring_key_length_fetch('SECRET_s2')
24
SELECT keyring_key_length_fetch('SECRET_s3');
keyring_key_length_fetch('SECRET_s3')
29
# Restarting the server
# keyring_key_fetch tests after restart
SELECT keyring_key_fetch('SECRET_g1') INTO @secret_g1_fetched_after_restart;
SELECT keyring_key_fetch('SECRET_g2') INTO @secret_g2_fetched_after_restart;
SELECT keyring_key_fetch('SECRET_g3') INTO @secret_g3_fetched_after_restart;
SELECT LENGTH(@secret_g1_fetched_after_restart);
LENGTH(@secret_g1_fetched_after_restart)
16
SELECT LENGTH(@secret_g2_fetched_after_restart);
LENGTH(@secret_g2_fetched_after_restart)
32
SELECT LENGTH(@secret_g3_fetched_after_restart);
LENGTH(@secret_g3_fetched_after_restart)
64
SELECT keyring_key_fetch('SECRET_s1');
keyring_key_fetch('SECRET_s1')
Quick brown fox jumped over the lazy dog
SELECT keyring_key_fetch('SECRET_s2');
keyring_key_fetch('SECRET_s2')
Old MacDonald had a farm
SELECT keyring_key_fetch('SECRET_s3');
keyring_key_fetch('SECRET_s3')
Quis custodiet ipsos custodes
# keyring key_type_fetch tests after restart
SELECT keyring_key_type_fetch('SECRET_g1');
keyring_key_type_fetch('SECRET_g1')
SECRET
SELECT keyring_key_type_fetch('SECRET_g2');
keyring_key_type_fetch('SECRET_g2')
SECRET
SELECT keyring_key_type_fetch('SECRET_g3');
keyring_key_type_fetch('SECRET_g3')
SECRET
# keyring key_length_fetch tests after restart
SELECT keyring_key_length_fetch('SECRET_g1');
keyring_key_length_fetch('SECRET_g1')
16
SELECT keyring_key_length_fetch('SECRET_g2');
keyring_key_length_fetch('SECRET_g2')
32
SELECT keyring_key_length_fetch('SECRET_g3');
keyring_key_length_fetch('SECRET_g3')
64
# keyring_key_remove tests
SELECT keyring_key_remove('SECRET_g1');
keyring_key_remove('SECRET_g1')
1
SELECT keyring_key_remove('SECRET_g2');
keyring_key_remove('SECRET_g2')
1
SELECT keyring_key_remove('SECRET_g3');
keyring_key_remove('SECRET_g3')
1
SELECT keyring_key_remove('SECRET_s1');
keyring_key_remove('SECRET_s1')
1
SELECT keyring_key_remove('SECRET_s2');
keyring_key_remove('SECRET_s2')
1
SELECT keyring_key_remove('SECRET_s3');
keyring_key_remove('SECRET_s3')
1
# ----------------------------------------------------------------------
# Clean-up
DROP FUNCTION keyring_key_store;
DROP FUNCTION keyring_key_fetch;
DROP FUNCTION keyring_key_remove;
DROP FUNCTION keyring_key_generate;
DROP FUNCTION keyring_key_type_fetch;
DROP FUNCTION keyring_key_length_fetch;
UNINSTALL PLUGIN keyring_udf;
# ----------------------------------------------------------------------
# ----------------------------------------------------------------------
# Teardown
# Removing manifest file for current MySQL server instance
# Removing local keyring file for keyring component: component_keyring_file
# Removing local configuration file for keyring component: component_keyring_file
# Restarting server without the manifest file
# ----------------------------------------------------------------------

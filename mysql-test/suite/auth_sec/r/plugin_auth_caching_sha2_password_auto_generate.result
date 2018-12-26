# Setup
# Test 1: System variable/Status variable tests
# Restart completed.
# Search for : Skipping generation of RSA key pair as --sha256_password_auto_generate_rsa_keys is set to OFF.
Pattern "Skipping generation of RSA key pair as --sha256_password_auto_generate_rsa_keys is set to OFF." found
# Search for : Skipping generation of RSA key pair as --caching_sha2_password_auto_generate_rsa_keys is set to OFF.
Pattern "Skipping generation of RSA key pair as --caching_sha2_password_auto_generate_rsa_keys is set to OFF." found
# Search completed.
# caching_sha2_password_auto_generate_rsa_keys should be OFF.
select @@global.caching_sha2_password_auto_generate_rsa_keys;
@@global.caching_sha2_password_auto_generate_rsa_keys
0
# caching_sha2_password_auto_generate_rsa_keys can not be changed at runtime
set @@global.caching_sha2_password_auto_generate_rsa_keys='OFF';
ERROR HY000: Variable 'caching_sha2_password_auto_generate_rsa_keys' is a read only variable
# Check caching_sha2_password_rsa_public_key status variable
show status like 'Caching_sha2_password_rsa_public_key';
Variable_name	Value
Caching_sha2_password_rsa_public_key	-----BEGIN PUBLIC KEY-----
MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAvV2VNbsQPG0Bh0KC8F4z
CGXvMNcSicCiLXxeLWrJsmKZl0ggf2ydymYUUewq+dVxDdh85sdSvxEmtIWvKSRK
+RRCAURztq2Succd+24SF5IZYjlIJE/U0AYUxHzUcOsannfzui60IaTHpcBFHTJK
6myxGx9MORZmhfv580mfvz4yvgLjS5yGOIS6rlxD9YV1Y04Rx3SXQQBnC7rDBL91
ktNWvbclsonfytY19N9p+Gprms30yRT+BmPFB7TqpReeZa3ivg15g/z3BLNyvj3Y
KiQM3cd7ENJC2x2LRxL5pG684cFNStSjT4FvA+oh45UnU45aOSEjrxNkBG8ci0e+
VKX539rK+nDzTE/MHpnvfHp4DB+kSYBPuKHY2Eaw31NwPpfLWwEJPiDrktJJmRZq
ENMHLXksdiqGhvYmI33wZaZAfjbDZFMfPF5yBMBGDZ3aeNz5Le7uqS6g6XMOoiz/
d2S5RzRrCol1yqCBPtODjfFPC4K8GGYVkWZgSCf/PRt/DgDnZOfZSSYIQNeyr21e
mqgqQ+yhXEGKVjcDTKcbSLiWAdA+GkAzLAXXhafM8mrhpnGKdO4Or6ySz7G1vk2J
t2ZSdP740oVSJi59P9NEgXcbd3c4FzjXSOOsxfhPQfobUk3ikt55lN3fBX3mBvUd
uxNhAcQ02ZD5zXrX6+loiV8CAwEAAQ==
-----END PUBLIC KEY-----

# Test 2: Restarting mysqld with
#   --caching_sha2_password_auto_generate_rsa_keys
# Restart completed.
# Search for : Skipping generation of RSA key pair as --sha256_password_auto_generate_rsa_keys is set to OFF.
Pattern "Skipping generation of RSA key pair as --sha256_password_auto_generate_rsa_keys is set to OFF." found
# Search for : Auto generated RSA key files through --caching_sha2_password_auto_generate_rsa_keys are placed in data directory.
Pattern "Auto generated RSA key files through --caching_sha2_password_auto_generate_rsa_keys are placed in data directory." found
# Search completed.
# Search for RSA key files in Data directory.
# Search completed.
# Test 3: Restarting mysqld with
#   --sha256_password_auto_generate_rsa_keys=ON
#   --caching_sha2_password_auto_generate_rsa_keys=ON
# Restart completed.
# Search for : Auto generated RSA key files through --sha256_password_auto_generate_rsa_keys are placed in data directory.
Pattern "Auto generated RSA key files through --sha256_password_auto_generate_rsa_keys are placed in data directory." found
# Search for : Skipping generation of RSA key pair through --caching_sha2_password_auto_generate_rsa_keys as options related to RSA keys are specified.
Pattern "Skipping generation of RSA key pair through --caching_sha2_password_auto_generate_rsa_keys as options related to RSA keys are specified." found
# Search completed.
# Clean-up

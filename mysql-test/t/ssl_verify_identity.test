# === Purpose ===
# This test verifies that while verifying the server certificates
# when ssl-mode=VERIFY_IDENTITY, the DNS/IPs provided in the Subject
# Alternative Names (which can be provided as an extension in X509)
# fields are also checked for apart from the Common Name in the subject.
# Applicable for openssl versions 1.0.2 and greater.
#
# === Related bugs and/or worklogs ===
# Bug #16211011 - SSL CERTIFICATE SUBJECT ALT NAMES WITH IPS NOT RESPECTED WITH ssl-mode=VERIFY_IDENTITY
#
# Note that these test cases are written keeping in mind that the openssl version used by the system will
# be 1.0.2+. For older versions of openssl, the test will be skipped.

--source include/check_openssl_version.inc
--source include/allowed_ciphers.inc

let PARAM_TEST_EXE=$MYSQL ;
let PARAM_CIPHER_VARIABLE=Ssl_cipher;
let PARAM_VERIFY_IDENTITY_ERROR=ERROR 2005 \(HY000\): Unknown MySQL server host 'nonexistent';
--source include/test_ssl_verify_identity.inc

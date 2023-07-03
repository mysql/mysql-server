<<<<<<< HEAD
# Only run this test if OpenSSL is supported
let $shavars= query_get_value("SHOW STATUS LIKE 'Rsa_public_key'", Variable_name, 1);
if ($shavars != 'Rsa_public_key'){
  skip Need OpenSSL support;
}
<<<<<<< HEAD
--source include/allowed_ciphers.inc
=======
=======
-- require r/openssl.require
disable_query_log;
show variables like "have_openssl";
enable_query_log;
--source include/allowed_ciphers.inc
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

# openssl 1.0.x behaves differently wrt sending session tickets.
# this file is for openssl 1.1.x (that has TLS1.3)
--let $openssl_binary_version = 1\\.1.*
source include/have_openssl_binary_version.inc;
source include/have_tlsv13.inc;

--echo #
--echo # WL#13075: Support TLS session reuse in the C API tls v1.2
--echo #

let $tls_version=tlsv1.2;
source include/ssl_cache.inc;

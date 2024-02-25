# openssl 1.0.x behaves differently wrt sending session tickets.
# this file is for openssl 1.0.x (that doesn't have TLS1.3)
--let $openssl_binary_version = 1\\.0.*
source include/have_openssl_binary_version.inc;
source include/not_tlsv13.inc;

--echo #
--echo # WL#13075: Support TLS session reuse in the C API tls v1.2
--echo #

--echo # FR8.1: openssl 1.0.x behavior.
let $tls_version=tlsv1.2;
source include/ssl_cache.inc;

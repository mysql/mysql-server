--echo #
--echo # WL#13075: Support TLS session reuse in the C API TLS v1.3
--echo #
--let $openssl_binary_version = 1\\.1.*
source include/have_openssl_binary_version.inc;
source include/have_tlsv13.inc;

let $tls_version=tlsv1.3;
source include/ssl_cache.inc;

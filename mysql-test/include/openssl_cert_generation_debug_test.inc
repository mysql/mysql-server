#
# Parameters:
#
# $debug_pattern
# $error_log
# $error_pattern
# $generate_ssl_certs
# $generate_rsa_keys

--echo # Restarting server with --debug option
--error 1
--exec $MYSQLD_CMD --ssl-ca="" --ssl-cert="" --ssl-key="" --auto_generate_certs=$generate_ssl_certs --sha256_password_auto_generate_rsa_keys=$generate_rsa_keys --debug=$debug_pattern --loose-console --loose-standalone > $error_log 2>&1
let SEARCH_PATTERN=$error_pattern;
--source include/search_pattern.inc

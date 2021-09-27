call mtr.add_suppression("Failed to set up SSL because of the following SSL library error");
# WL#14481: deprecated --ssl
include/assert_grep.inc [Found a --ssl deprecation message in the log]
################## FR8: --ssl only applies to startups
# Check if ssl is off: must be off.
# reset and enable SSL
ALTER INSTANCE RELOAD TLS;
# SSL must be enabled now despite the value of --ssl
1
1
# cleanup
SET @orig_ssl_ca= @@global.ssl_ca;
SET GLOBAL ssl_ca = 'gizmo';
ALTER INSTANCE RELOAD TLS NO ROLLBACK ON ERROR;
Warnings:
Warning	3888	Failed to set up SSL because of the following SSL library error: SSL_CTX_set_default_verify_paths failed
SET GLOBAL ssl_ca = @orig_ssl_ca;
################## End of dynamic SSL tests

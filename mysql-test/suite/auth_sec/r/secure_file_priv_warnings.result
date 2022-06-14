#-----------------------------------------------------------------------
# Setup
#-----------------------------------------------------------------------
# restart:--log-error=MYSQLTEST_VARDIR/log/sfpw.err
# Search for : Insecure configuration for --secure-file-priv: Current
#              value does not restrict location of generated files.
#              Consider setting it to a valid, non-empty path.
Pattern "Insecure configuration for --secure-file-priv: Current value does not restrict location of generated files. Consider setting it to a valid, non-empty path." found
SHOW VARIABLES LIKE 'secure_file_priv';
Variable_name	Value
secure_file_priv	
#-----------------------------------------------------------------------
# restart:--log-error=MYSQLTEST_VARDIR/log/sfpw.err --plugin-dir=PLUGIN_DIR --secure-file-priv=PLUGIN_DIR
# Search for : Insecure configuration for --secure-file-priv: Plugin
#              directory is accessible through --secure-file-priv.
#              Consider choosing a different directory.
Pattern "Insecure configuration for --secure-file-priv: Plugin directory is accessible through --secure-file-priv. Consider choosing a different directory." found
#-----------------------------------------------------------------------
# Clean-up
#-----------------------------------------------------------------------
# restart:

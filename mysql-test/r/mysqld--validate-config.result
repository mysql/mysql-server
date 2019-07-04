# This test covers the 'validate-config' option.
# Stop the server which was started by MTR.
# Reports an error since --foo is an invalid option
Pattern "unknown variable 'foo=bar'" found
# Reports an error since --tx_read_only is an invalid option
Pattern "unknown variable 'tx_read_only=on'" found
# No error reported.
# No error reported. Only warning about invalid value.
# No error or warning reported for invalid value for SE option.
Pattern "option 'read_only': boolean value 's' was not recognized. Set to OFF." found
# Restart the server with default options.
# restart

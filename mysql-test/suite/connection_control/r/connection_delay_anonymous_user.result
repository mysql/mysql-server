# Connection delay tests for valid user accounts

# ----------------------------------------------------------------------

# Setup
# Install connection_control plugin
INSTALL PLUGIN connection_control SONAME 'CONNECTION_CONTROL_LIB';
INSTALL PLUGIN connection_control_failed_login_attempts SONAME 'CONNECTION_CONTROL_LIB';
# Create anonymous user
CREATE USER ''@'localhost';
# Save original values of connection_control variables
SET @saved_connections_threshold = @@global.connection_control_failed_connections_threshold;
SET @saved_max_delay = @@global.connection_control_max_connection_delay;
# Set small values for connection_control variables
SET @@global.connection_control_failed_connections_threshold = 3;
SET @global.connection_control_max_connection_delay = 1000;

# ----------------------------------------------------------------------

# Following attempts will not experience any delay in server respose
# Connection attempt should fail.
Connection_control_delay_generated should be 0
Variable_name	Value
Connection_control_delay_generated	0
# Connection attempt should fail.
Connection_control_delay_generated should be 0
Variable_name	Value
Connection_control_delay_generated	0
# Connection attempt should fail.
Connection_control_delay_generated should be 0
Variable_name	Value
Connection_control_delay_generated	0

# Following attempts will experience delay in server respose
# Connection attempt should fail.
Connection_control_delay_generated should be 1
Variable_name	Value
Connection_control_delay_generated	1
# Connection attempt should fail.
Connection_control_delay_generated should be 2
Variable_name	Value
Connection_control_delay_generated	2
# Connection attempt should fail.
Connection_control_delay_generated should be 3
Variable_name	Value
Connection_control_delay_generated	3

# ----------------------------------------------------------------------

# Cleanup
# Restore original values of conenction_control variables
SET @@global.connection_control_failed_connections_threshold = @saved_connections_threshold;
SET @@global.connection_control_max_connection_delay = @saved_max_delay;
# Drop anonymous user
DROP USER ''@'localhost';
# Uninstall connection_control plugin
UNINSTALL PLUGIN connection_control;
UNINSTALL PLUGIN connection_control_failed_login_attempts;

# ----------------------------------------------------------------------

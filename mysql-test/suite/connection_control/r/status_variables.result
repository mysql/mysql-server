# Connection delay tests for valid user accounts

# ----------------------------------------------------------------------

# Setup
# Install connection_control plugin
INSTALL PLUGIN connection_control SONAME 'CONNECTION_CONTROL_LIB';
INSTALL PLUGIN connection_control_failed_login_attempts SONAME 'CONNECTION_CONTROL_LIB';
# Create user accounts for testing
CREATE USER u1@localhost IDENTIFIED BY 'abcd';
CREATE USER u2@localhost IDENTIFIED BY 'abcd';
CREATE USER u3@localhost IDENTIFIED BY 'abcd';
# Save original values of connection_control variables
SET @saved_connections_threshold = @@global.connection_control_failed_connections_threshold;
SET @saved_max_delay= @@global.connection_control_max_connection_delay;
# Set small threshold
SET @@global.connection_control_failed_connections_threshold = 1;
# Set small max delay
SET @@global.connection_control_max_connection_delay = 1000;

# ----------------------------------------------------------------------

connect(localhost,u1,,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u1'@'localhost' (using password: NO)
connect(localhost,u3,haha,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u3'@'localhost' (using password: YES)
connect(localhost,u2,efgh,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u2'@'localhost' (using password: YES)
# Check  Connection_control_delay_generated - Should be 0
SHOW STATUS LIKE 'Connection_control_delay_generated';
Variable_name	Value
Connection_control_delay_generated	0
connect(localhost,u1,,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u1'@'localhost' (using password: NO)
connect(localhost,u2,haha,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u2'@'localhost' (using password: YES)
connect(localhost,u3,efgh,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u3'@'localhost' (using password: YES)
connect(localhost,u2,,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u2'@'localhost' (using password: NO)
connect(localhost,u1,haha,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u1'@'localhost' (using password: YES)
connect(localhost,u3,efgh,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u3'@'localhost' (using password: YES)
# Check Connection_control_delay_generated - Should be 6
SHOW STATUS LIKE 'Connection_control_delay_generated';
Variable_name	Value
Connection_control_delay_generated	6
connect(localhost,u2,,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u2'@'localhost' (using password: NO)
connect(localhost,u1,haha,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u1'@'localhost' (using password: YES)
connect(localhost,u3,efgh,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u3'@'localhost' (using password: YES)
# Check Connection_control_delay_generated - Should be 9
SHOW STATUS LIKE 'Connection_control_delay_generated';
Variable_name	Value
Connection_control_delay_generated	9
# Successful connection but delays would still be generated
# Should be 12
SHOW STATUS LIKE 'Connection_control_delay_generated';
Variable_name	Value
Connection_control_delay_generated	12
# Setting failed connection threshold should reset delay statistics
SET @@global.connection_control_failed_connections_threshold = 1;
connect(localhost,u1,,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u1'@'localhost' (using password: NO)
connect(localhost,u3,haha,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u3'@'localhost' (using password: YES)
connect(localhost,u2,efgh,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u2'@'localhost' (using password: YES)
connect(localhost,u1,,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u1'@'localhost' (using password: NO)
connect(localhost,u3,haha,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u3'@'localhost' (using password: YES)
connect(localhost,u2,efgh,test,MASTER_PORT,MASTER_SOCKET);
ERROR 28000: Access denied for user 'u2'@'localhost' (using password: YES)
# Check Connection_control_delay_generated - Should be 3
SHOW STATUS LIKE 'Connection_control_delay_generated';
Variable_name	Value
Connection_control_delay_generated	3

# ----------------------------------------------------------------------

# Cleanup
# Restore original values of conenction_control variables
SET @@global.connection_control_max_connection_delay= @saved_max_delay;
SET @@global.connection_control_failed_connections_threshold = @saved_connections_threshold;
# Remove user accounts created for the test
DROP USER u1@localhost;
DROP USER u2@localhost;
DROP USER u3@localhost;
# Uninstall connection_control plugin
UNINSTALL PLUGIN connection_control;
UNINSTALL PLUGIN connection_control_failed_login_attempts;

# ----------------------------------------------------------------------

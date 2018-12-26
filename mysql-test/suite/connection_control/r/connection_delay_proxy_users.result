#-----------------------------------------------------------------------
# Setup
# Install connection_control plugin
INSTALL PLUGIN connection_control SONAME 'CONNECTION_CONTROL_LIB';
INSTALL PLUGIN connection_control_failed_login_attempts SONAME 'CONNECTION_CONTROL_LIB';
# Do proxy setup
CALL mtr.add_suppression("Plugin test_plugin_server reported: 'Wrong password supplied for proxied'");
SET @saved_connections_threshold = @@global.connection_control_failed_connections_threshold;
SET @saved_max_delay = @@global.connection_control_max_connection_delay;
SET @saved_min_delay= @@global.connection_control_min_connection_delay;
SET @@global.connection_control_failed_connections_threshold = 3;
SET @@global.connection_control_max_connection_delay = 4000;
SET @@global.connection_control_min_connection_delay = 2000;
#-----------------------------------------------------------------------
# Case 1 : Testss for valid user accounts
# Make enough failed attempts to trigger delays
connect(localhost,u1,,test,MASTER_PORT,MASTER_SOCKET);
connect(localhost,u1,haha,test,MASTER_PORT,MASTER_SOCKET);
connect(localhost,u2,efgh,test,MASTER_PORT,MASTER_SOCKET);
connect(localhost,u3,,test,MASTER_PORT,MASTER_SOCKET);
connect(localhost,u2,haha,test,MASTER_PORT,MASTER_SOCKET);
connect(localhost,u3,efgh,test,MASTER_PORT,MASTER_SOCKET);
connect(localhost,u2,,test,MASTER_PORT,MASTER_SOCKET);
connect(localhost,u1,haha,test,MASTER_PORT,MASTER_SOCKET);
connect(localhost,u3,efgh,test,MASTER_PORT,MASTER_SOCKET);
# Following attempts will experience delay in server respose
# Connection attempt should fail.
Connection_control_delay_generated should be 1
Variable_name	Value
Connection_control_delay_generated	1
@server_response_time >= 2
1
# Connection attempt should fail.
Connection_control_delay_generated should be 2
Variable_name	Value
Connection_control_delay_generated	2
@server_response_time >= 2
1
# Connection attempt should fail.
Connection_control_delay_generated should be 3
Variable_name	Value
Connection_control_delay_generated	3
@server_response_time >= 2
1
# Connection attempt should succeed.
Connection_control_delay_generated should be 4
Variable_name	Value
Connection_control_delay_generated	4
@server_response_time >= 2
1
# Connection attempt should succeed.
Connection_control_delay_generated should be 5
Variable_name	Value
Connection_control_delay_generated	5
@server_response_time >= 2
1
# Connection attempt should succeed.
Connection_control_delay_generated should be 6
Variable_name	Value
Connection_control_delay_generated	6
@server_response_time >= 2
1
# Following attempts will not experience any delay in server respose
# Connection attempt should fail.
Connection_control_delay_generated should be 6
Variable_name	Value
Connection_control_delay_generated	6
# Connection attempt should fail.
Connection_control_delay_generated should be 6
Variable_name	Value
Connection_control_delay_generated	6
# Connection attempt should fail.
Connection_control_delay_generated should be 6
Variable_name	Value
Connection_control_delay_generated	6
# Connection attempt should succeed.
Connection_control_delay_generated should be 6
Variable_name	Value
Connection_control_delay_generated	6
# Connection attempt should succeed.
Connection_control_delay_generated should be 6
Variable_name	Value
Connection_control_delay_generated	6
# Connection attempt should succeed.
Connection_control_delay_generated should be 6
Variable_name	Value
Connection_control_delay_generated	6
#-----------------------------------------------------------------------
# Cleanup
SET @@global.connection_control_failed_connections_threshold = @saved_connections_threshold;
SET @@global.connection_control_max_connection_delay = @saved_max_delay;
SET @@global.connection_control_min_connection_delay = @saved_min_delay;
# Remove user accounts
# Uninstall connection_control plugin
UNINSTALL PLUGIN connection_control;
UNINSTALL PLUGIN connection_control_failed_login_attempts;
#-----------------------------------------------------------------------

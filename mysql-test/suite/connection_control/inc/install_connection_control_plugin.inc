# Install connection_control plugin
--replace_result $CONNECTION_CONTROL_PLUGIN CONNECTION_CONTROL_LIB
eval INSTALL PLUGIN connection_control SONAME '$CONNECTION_CONTROL_PLUGIN';
--replace_result $CONNECTION_CONTROL_PLUGIN CONNECTION_CONTROL_LIB
eval INSTALL PLUGIN connection_control_failed_login_attempts SONAME '$CONNECTION_CONTROL_PLUGIN'; 

#
# bug#30209760 : ASSERTION FAILURE: SRV0START.CC:969:SUCCESS THREAD
#
#########
# SETUP #
#########
# Create path for ibdata* & undo* files
# create bootstrap file
# Stop the MTR default DB server
#########################################################################
# INITIALIZE NEW SERVER
#########################################################################
# Server should be initialized successfully.
#########################################################################
# RESTART 2 : WITH KEYRING PLUGIN.
innodb_undo_log_encrypt=ON.
Crash server before purge thread could start.
#########################################################################
#########################################################################
# RESTART 3 : WITH KEYRING PLUGIN.
innodb_undo_log_encrypt=ON.
Skip rotating default master key in master thread
#########################################################################
#########################################################################
# RESTART 4 : WITH KEYRING PLUGIN.
innodb_undo_log_encrypt=OFF.
#########################################################################
# Shutdown the running server
SHUTDOWN;
###########
# CLEANUP #
###########
# Restart the server with MTR default

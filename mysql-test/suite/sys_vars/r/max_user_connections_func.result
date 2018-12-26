** Setup **

SET @default_max_user_connections = @@global.max_user_connections;
Set Global max_user_connections=2;
'#--------------------FN_DYNVARS_114_01-------------------------#'
** Connecting conn1 using username 'root' **
** Connecting conn2 using username 'root' **
** Connecting conn3 using username 'root' **
ERROR 42000: User root already has more than 'max_user_connections' active connections
Expected error "too many connections"
** Disconnecting conn1 **
** Poll till disconnected conn1 disappears from processlist
'#--------------------FN_DYNVARS_114_02-------------------------#'
Set Global max_user_connections=3;
** Connecting conn5 using username 'root' **
** Connecting conn6 using username 'root' **
** Connection default **
** Disconnecting conn5, conn6 **
SET @@global.max_user_connections = @default_max_user_connections;

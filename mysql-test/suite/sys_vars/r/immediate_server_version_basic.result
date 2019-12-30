SELECT @@GLOBAL.immediate_server_version;
ERROR HY000: Variable 'immediate_server_version' is a SESSION variable
include/assert.inc [immediate_server_version must be accessible and have the default value]
SHOW VARIABLES LIKE 'immediate_server_version';
Variable_name	Value
immediate_server_version	999999
SET @@GLOBAL.immediate_server_version = 123456;
ERROR HY000: Variable 'immediate_server_version' is a SESSION variable and can't be used with SET GLOBAL
SET @@SESSION.immediate_server_version = 123456;
include/assert.inc [immediate_server_version must be equal to the set value]
SET @@SESSION.immediate_server_version = -123456;
Warnings:
Warning	1292	Truncated incorrect immediate_server_version value: '-123456'
include/assert.inc [immediate_server_version must be 0]
SET @@SESSION.immediate_server_version = 0;
include/assert.inc [immediate_server_version must be 0]
SET @@SESSION.immediate_server_version = 123456789012345678901;
ERROR 42000: Incorrect argument type to variable 'immediate_server_version'
include/assert.inc [immediate_server_version must be 0]
SET @@SESSION.immediate_server_version = 'xyz';
ERROR 42000: Incorrect argument type to variable 'immediate_server_version'
include/assert.inc [immediate_server_version must be 0]
SET @@SESSION.immediate_server_version = xyz;
ERROR 42000: Incorrect argument type to variable 'immediate_server_version'
include/assert.inc [immediate_server_version must be 0]
SET @@SESSION.immediate_server_version = 36028797018963969;
Warnings:
Warning	1292	Truncated incorrect immediate_server_version value: '36028797018963969'
include/assert.inc [immediate_server_version must be equal to the maximum value]
SET @@SESSION.immediate_server_version = 18446744073709551615;
Warnings:
Warning	1292	Truncated incorrect immediate_server_version value: '18446744073709551615'
include/assert.inc [immediate_server_version must be equal to the maximum value]
SET @@SESSION.immediate_server_version = 18446744073709551616;
ERROR 42000: Incorrect argument type to variable 'immediate_server_version'
include/assert.inc [immediate_server_version must be equal to the maximum value]
include/assert.inc [The value in session table matches the one in the variable]
include/assert.inc [There is only one variable with this name]
include/assert.inc [There is only one variable with this name in the performance schema table]
include/assert.inc [The variable can be accessed with @@ sign]
CREATE USER nosuper;
SET SESSION immediate_server_version = 0;
ERROR 42000: Access denied; you need (at least one of) the SUPER, SYSTEM_VARIABLES_ADMIN, SESSION_VARIABLES_ADMIN or REPLICATION_APPLIER privilege(s) for this operation
DROP USER nosuper;

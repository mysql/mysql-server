# Default values
SELECT @@GLOBAL.default_password_lifetime;
@@GLOBAL.default_password_lifetime
0
SELECT @@SESSION.default_password_lifetime;
ERROR HY000: Variable 'default_password_lifetime' is a GLOBAL variable
SET @saved_value = @@global.default_password_lifetime;
# Valid values
SET GLOBAL default_password_lifetime = 0;
SET GLOBAL default_password_lifetime = 1;
SET GLOBAL default_password_lifetime = 65535;
SET GLOBAL default_password_lifetime = 9999;
# Invalid values: there shall be warnings about truncation
SET GLOBAL default_password_lifetime = -1;
Warnings:
Warning	1292	Truncated incorrect default_password_lifetime value: '-1'
SET GLOBAL default_password_lifetime = -1024;
Warnings:
Warning	1292	Truncated incorrect default_password_lifetime value: '-1024'
SET GLOBAL default_password_lifetime = 65536;
Warnings:
Warning	1292	Truncated incorrect default_password_lifetime value: '65536'
SET GLOBAL default_password_lifetime = 99999;
Warnings:
Warning	1292	Truncated incorrect default_password_lifetime value: '99999'
SET GLOBAL default_password_lifetime = @saved_value;

** Setup **

SET @default_check_proxy_users = @@check_proxy_users;
'#----- 1.2.4 superuser set variable -------------------------#'
SET Global check_proxy_users=OFF;
'#----- 1.2.4 others user set variable -----------------------#'
** Creating new user with out super privilege**
CREATE USER sameea;
** Connecting connn using username 'sameea' **
SET GLOBAL check_proxy_users=ON;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET @@global.check_proxy_users=ON;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
** Connection default **
** Disconnecting connn **
DROP USER sameea;
SET global check_proxy_users = @default_check_proxy_users;

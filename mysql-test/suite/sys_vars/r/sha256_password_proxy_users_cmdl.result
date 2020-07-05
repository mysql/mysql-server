SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
1
'#----- 3.1.1 Valid values on cmd line -----------------------#'
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
1
'#----- 3.1.2 Valid values on cmd line -----------------------#'
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
1
'#----- 3.1.3 Invalid values on cmd line -----------------------#'
CALL mtr.add_suppression("option 'sha256_password_proxy_users': boolean value '-1' was not recognized. Set to OFF.");
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
CALL mtr.add_suppression("option 'sha256_password_proxy_users': boolean value '43698' was not recognized. Set to OFF.");
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
CALL mtr.add_suppression("option 'sha256_password_proxy_users': boolean value 'TEST' was not recognized. Set to OFF.");
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
'#----- 3.1.4 Invalid values on cmd line -----------------------#'
CALL mtr.add_suppression("option 'sha256_password_proxy_users': boolean value ''test'' was not recognized. Set to OFF.");
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
'#----- 3.1.5 empty values on cmd line -----------------------#'
CALL mtr.add_suppression("option 'sha256_password_proxy_users': boolean value '' was not recognized. Set to OFF.");
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
# restart:

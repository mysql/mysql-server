SELECT @@global.check_proxy_users;
@@global.check_proxy_users
1
'#----- 1.1.1 Valid values on cmd line -----------------------#'
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
1
'#----- 1.1.2 Valid values on cmd line -----------------------#'
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
1
'#----- 1.1.3 Invalid values on cmd line -----------------------#'
CALL mtr.add_suppression("option 'check_proxy_users': boolean value '-1' was not recognized. Set to OFF.");
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
CALL mtr.add_suppression("option 'check_proxy_users': boolean value '43698' was not recognized. Set to OFF.");
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
CALL mtr.add_suppression("option 'check_proxy_users': boolean value 'TEST' was not recognized. Set to OFF.");
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
'#----- 1.1.4 Invalid values on cmd line -----------------------#'
CALL mtr.add_suppression("option 'check_proxy_users': boolean value ''test'' was not recognized. Set to OFF.");
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
'#----- 1.1.5 empty values on cmd line -----------------------#'
CALL mtr.add_suppression("option 'check_proxy_users': boolean value '' was not recognized. Set to OFF.");
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
# restart:

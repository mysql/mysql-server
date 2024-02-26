------------------------------------------------------------------------
# Setup
CREATE DATABASE db1;
CREATE TABLE db1.t1(c1 int);
INSERT INTO db1.t1 VALUES (1), (2), (3);
------------------------------------------------------------------------
# Case: - Connecting user has sufficient privileges
#       - Effective user does not have required privileges
CREATE USER qa_test_3_user IDENTIFIED WITH qa_auth_interface AS 'qa_test_3_dest';
GRANT SELECT ON *.* TO qa_test_3_user;
CREATE USER qa_test_3_dest IDENTIFIED BY 'dest_passwd';
GRANT SELECT ON *.* TO qa_test_3_dest;
REVOKE SELECT ON db1.* FROM qa_test_3_dest;
GRANT PROXY ON qa_test_3_dest TO qa_test_3_user;
exec MYSQL PLUGIN_AUTH_OPT -h localhost -P MASTER_MYPORT -u qa_test_3_user --password=qa_test_3_dest -e "SELECT current_user(),user(),@@local.proxy_user,@@local.external_user\G" 2>&1
mysql: [Warning] Using a password on the command line interface can be insecure.
*************************** 1. row ***************************
       current_user(): qa_test_3_dest@%
               user(): qa_test_3_user@localhost
   @@local.proxy_user: 'qa_test_3_user'@'%'
@@local.external_user: qa_test_3_dest
exec MYSQL PLUGIN_AUTH_OPT -h localhost -P MASTER_MYPORT -u qa_test_3_user --password=qa_test_3_dest -e "SELECT COUNT(*) FROM db1.t1\G" 2>&1
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 1142 (42000) at line 1: SELECT command denied to user 'qa_test_3_dest'@'localhost' for table 't1'
DROP USER qa_test_3_dest, qa_test_3_user;
------------------------------------------------------------------------
# Case: - Connecting user does not have required privileges
#       - Effective user has sufficient privileges
CREATE USER qa_test_4_user IDENTIFIED WITH qa_auth_interface AS 'qa_test_4_dest';
GRANT SELECT ON *.* TO qa_test_4_user;
REVOKE SELECT ON db1.* FROM qa_test_4_user;
CREATE USER qa_test_4_dest IDENTIFIED BY 'dest_passwd';
GRANT SELECT ON *.* TO qa_test_4_dest;
GRANT PROXY ON qa_test_4_dest TO qa_test_4_user;
exec MYSQL PLUGIN_AUTH_OPT -h localhost -P MASTER_MYPORT -u qa_test_4_user --password=qa_test_4_dest -e "SELECT current_user(),user(),@@local.proxy_user,@@local.external_user\G" 2>&1
mysql: [Warning] Using a password on the command line interface can be insecure.
*************************** 1. row ***************************
       current_user(): qa_test_4_dest@%
               user(): qa_test_4_user@localhost
   @@local.proxy_user: 'qa_test_4_user'@'%'
@@local.external_user: qa_test_4_dest
exec MYSQL PLUGIN_AUTH_OPT -h localhost -P MASTER_MYPORT -u qa_test_4_user --password=qa_test_4_dest -e "SELECT COUNT(*) FROM db1.t1;" 2>&1
mysql: [Warning] Using a password on the command line interface can be insecure.
COUNT(*)
3
DROP USER qa_test_4_dest, qa_test_4_user;
------------------------------------------------------------------------
# Cleanup
DROP TABLE db1.t1;
DROP DATABASE db1;
------------------------------------------------------------------------

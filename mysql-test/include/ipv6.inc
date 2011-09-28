eval CREATE USER testuser@'$IPv6' identified by '1234';
eval GRANT ALL ON test.* TO testuser@'$IPv6';
eval SHOW GRANTS FOR testuser@'$IPv6';
--replace_result ::1 localhost
SELECT USER();
--replace_result ::1 localhost
SELECT current_user();
--disable_result_log
SHOW PROCESSLIST;
--enable_result_log
connect (con1, $IPv6, root, , test, $MASTER_MYPORT);
connection default;
disconnect con1;
eval REVOKE ALL ON test.* FROM testuser@'$IPv6';
eval RENAME USER testuser@'$IPv6' to testuser1@'$IPv6';
eval SET PASSWORD FOR testuser1@'$IPv6' = PASSWORD ('9876');
--replace_result ::1 localhost
SELECT USER();
eval DROP USER testuser1@'$IPv6';


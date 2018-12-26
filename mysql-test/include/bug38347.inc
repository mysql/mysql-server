
--echo
SHOW GRANTS FOR mysqltest_u1@localhost;

--echo
--echo # connection: con1 (mysqltest_u1@mysqltest_db1)
--connect (con1,localhost,mysqltest_u1,,mysqltest_db1)
--connection con1

--echo
SHOW CREATE TABLE t1;

--echo
--echo # connection: default
--connection default

--disconnect con1

--echo
REVOKE ALL PRIVILEGES, GRANT OPTION FROM mysqltest_u1@localhost;
SHOW GRANTS FOR mysqltest_u1@localhost;

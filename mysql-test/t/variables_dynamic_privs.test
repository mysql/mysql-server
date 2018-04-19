CREATE USER u1@localhost IDENTIFIED BY 'foo';
CREATE USER u2@localhost IDENTIFIED BY 'foo';
CREATE ROLE r1, r2;
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO u1@localhost;
GRANT r1 TO u1@localhost;
GRANT r2 TO u2@localhost;
connect(con1, localhost, u1, foo, test);
--echo ++ Connected as u1@localhost
SET ROLE r1;
SET GLOBAL binlog_cache_size=100;
SET GLOBAL binlog_cache_size=DEFAULT;

connect(con2, localhost, u2, foo, test);
--echo
--echo ++ Connected as u2@localhost
--echo ++ Setting global variables without SUPER or
--echo ++ SYSTEM_VARIABLES_ADMIN should fail.
SET ROLE r2;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL binlog_cache_size=100;
connection default;
disconnect con1;
disconnect con2;
DROP USER u1@localhost, u2@localhost;
DROP ROLE r2,r1;

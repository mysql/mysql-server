CREATE USER u1@localhost IDENTIFIED BY 'foo';
CREATE USER u2@localhost IDENTIFIED BY 'foo';
CREATE ROLE r1, r2;
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO u1@localhost;
GRANT r1 TO u1@localhost;
GRANT r2 TO u2@localhost;
++ Connected as u1@localhost
SET ROLE r1;
SET GLOBAL binlog_cache_size=100;
Warnings:
Warning	1292	Truncated incorrect binlog_cache_size value: '100'
SET GLOBAL binlog_cache_size=DEFAULT;

++ Connected as u2@localhost
++ Setting global variables without SUPER or
++ SYSTEM_VARIABLES_ADMIN should fail.
SET ROLE r2;
SET GLOBAL binlog_cache_size=100;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
DROP USER u1@localhost, u2@localhost;
DROP ROLE r2,r1;

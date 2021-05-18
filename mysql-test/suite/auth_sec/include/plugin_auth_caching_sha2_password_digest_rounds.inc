# WL#14281 Make digest rounds configurable

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

SELECT @@global.caching_sha2_password_digest_rounds;
CREATE USER wl_14281@localhost IDENTIFIED WITH caching_sha2_password BY 'WL_14281_PASSWORD';
SELECT SUBSTRING(authentication_string, 4,3) AS length FROM mysql.user WHERE user='wl_14281';
connect(wl_14281_conn,localhost,wl_14281,WL_14281_PASSWORD,test);
SELECT user FROM information_schema.USER_ATTRIBUTES;
disconnect wl_14281_conn;
--source include/wait_until_disconnected.inc
connection default;
DROP USER wl_14281@localhost;

--source include/wait_until_count_sessions.inc

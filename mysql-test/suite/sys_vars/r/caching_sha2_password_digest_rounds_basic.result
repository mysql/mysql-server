SELECT @@global.caching_sha2_password_digest_rounds;
@@global.caching_sha2_password_digest_rounds
5000
CREATE USER wl_14281_default@localhost IDENTIFIED WITH caching_sha2_password BY 'WL_14281_PASSWORD';
SELECT SUBSTRING(authentication_string, 4,3) AS length FROM mysql.user WHERE user='wl_14281_default';
length
005
SELECT user FROM information_schema.USER_ATTRIBUTES;
USER
wl_14281_default
SET PERSIST_ONLY caching_sha2_password_digest_rounds=7000;
# restart
SELECT @@global.caching_sha2_password_digest_rounds;
@@global.caching_sha2_password_digest_rounds
7000
CREATE USER wl_14281_7000@localhost IDENTIFIED WITH caching_sha2_password BY 'WL_14281_PASSWORD';
SELECT SUBSTRING(authentication_string, 4,3) AS length FROM mysql.user WHERE user='wl_14281_7000';
length
007
SELECT user FROM information_schema.USER_ATTRIBUTES;
USER
wl_14281_7000
SELECT user FROM information_schema.USER_ATTRIBUTES;
USER
wl_14281_default
SET PERSIST_ONLY caching_sha2_password_digest_rounds=999000;
# restart
SELECT @@global.caching_sha2_password_digest_rounds;
@@global.caching_sha2_password_digest_rounds
999000
CREATE USER wl_14281_xl@localhost IDENTIFIED WITH caching_sha2_password BY 'WL_14281_PASSWORD';
SELECT SUBSTRING(authentication_string, 4,3) AS length FROM mysql.user WHERE user='wl_14281_xl';
length
3E7
SELECT user FROM information_schema.USER_ATTRIBUTES;
USER
wl_14281_xl
SELECT user FROM information_schema.USER_ATTRIBUTES;
USER
wl_14281_7000
SELECT user FROM information_schema.USER_ATTRIBUTES;
USER
wl_14281_default
RESET PERSIST caching_sha2_password_digest_rounds;
# restart
SELECT @@global.caching_sha2_password_digest_rounds;
@@global.caching_sha2_password_digest_rounds
5000
SELECT SUBSTRING(authentication_string, 4,3) AS length FROM mysql.user WHERE user='wl_14281_default';
length
005
SELECT user FROM information_schema.USER_ATTRIBUTES;
USER
wl_14281_xl
SELECT user FROM information_schema.USER_ATTRIBUTES;
USER
wl_14281_7000
SELECT user FROM information_schema.USER_ATTRIBUTES;
USER
wl_14281_default
DROP USER wl_14281_default@localhost;
DROP USER wl_14281_7000@localhost;
DROP USER wl_14281_xl@localhost;

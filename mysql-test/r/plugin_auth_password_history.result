# Must pass with a warning: syntax accepted but meaningless for plugin auth
CREATE USER pwd_history_plugin@localhost IDENTIFIED WITH 'test_plugin_server' PASSWORD HISTORY 1;
Warnings:
Warning	3639	Non-zero password history clauses ignored for user 'pwd_history_plugin'@'localhost' as its authentication plugin test_plugin_server does not support password history
# Must have default password history
SHOW CREATE USER pwd_history_plugin@localhost;
CREATE USER for pwd_history_plugin@localhost
CREATE USER `pwd_history_plugin`@`localhost` IDENTIFIED WITH 'test_plugin_server' REQUIRE NONE PASSWORD EXPIRE DEFAULT ACCOUNT UNLOCK PASSWORD HISTORY DEFAULT PASSWORD REUSE INTERVAL DEFAULT PASSWORD REQUIRE CURRENT DEFAULT
# Must be 0: non-local passwords plugin
SELECT COUNT(*) FROM mysql.password_history WHERE
User='pwd_history_plugin' AND Host='localhost';
COUNT(*)
0
# Must pass: syntax accepted but meaningless for plugin auth
ALTER USER pwd_history_plugin@localhost IDENTIFIED WITH 'test_plugin_server' PASSWORD REUSE INTERVAL 1 DAY;
Warnings:
Warning	3639	Non-zero password history clauses ignored for user 'pwd_history_plugin'@'localhost' as its authentication plugin test_plugin_server does not support password history
# Must have default password reuse interval
SHOW CREATE USER pwd_history_plugin@localhost;
CREATE USER for pwd_history_plugin@localhost
CREATE USER `pwd_history_plugin`@`localhost` IDENTIFIED WITH 'test_plugin_server' REQUIRE NONE PASSWORD EXPIRE DEFAULT ACCOUNT UNLOCK PASSWORD HISTORY DEFAULT PASSWORD REUSE INTERVAL DEFAULT PASSWORD REQUIRE CURRENT DEFAULT
# Must be 0: non-local passwords plugin
SELECT COUNT(*) FROM mysql.password_history WHERE
User='pwd_history_plugin' AND Host='localhost';
COUNT(*)
0
DROP USER pwd_history_plugin@localhost;
CREATE USER mohit@localhost IDENTIFIED BY 'mohit_native' PASSWORD HISTORY 1;
# Must be 1: password history on
SELECT COUNT(*) FROM mysql.password_history WHERE
User='mohit' AND Host='localhost';
COUNT(*)
1
ALTER USER mohit@localhost IDENTIFIED WITH 'test_plugin_server' AS 'haha';
# Must have default password reuse interval
SHOW CREATE USER mohit@localhost;
CREATE USER for mohit@localhost
CREATE USER `mohit`@`localhost` IDENTIFIED WITH 'test_plugin_server' AS 'haha' REQUIRE NONE PASSWORD EXPIRE DEFAULT ACCOUNT UNLOCK PASSWORD HISTORY DEFAULT PASSWORD REUSE INTERVAL DEFAULT PASSWORD REQUIRE CURRENT DEFAULT
# Must be 0: password history on
SELECT COUNT(*) FROM mysql.password_history WHERE
User='mohit' AND Host='localhost';
COUNT(*)
0
DROP USER mohit@localhost;
# End of 8.0 tests

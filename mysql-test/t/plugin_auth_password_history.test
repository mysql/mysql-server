--source include/have_plugin_auth.inc

--echo # Must pass with a warning: syntax accepted but meaningless for plugin auth
CREATE USER pwd_history_plugin@localhost IDENTIFIED WITH 'test_plugin_server' PASSWORD HISTORY 1;

--echo # Must have default password history
SHOW CREATE USER pwd_history_plugin@localhost;

--echo # Must be 0: non-local passwords plugin
SELECT COUNT(*) FROM mysql.password_history WHERE
  User='pwd_history_plugin' AND Host='localhost';

--echo # Must pass: syntax accepted but meaningless for plugin auth
ALTER USER pwd_history_plugin@localhost IDENTIFIED WITH 'test_plugin_server' PASSWORD REUSE INTERVAL 1 DAY;

--echo # Must have default password reuse interval
SHOW CREATE USER pwd_history_plugin@localhost;

--echo # Must be 0: non-local passwords plugin
SELECT COUNT(*) FROM mysql.password_history WHERE
  User='pwd_history_plugin' AND Host='localhost';

DROP USER pwd_history_plugin@localhost;

CREATE USER mohit@localhost IDENTIFIED BY 'mohit_native' PASSWORD HISTORY 1;

--echo # Must be 1: password history on
SELECT COUNT(*) FROM mysql.password_history WHERE
  User='mohit' AND Host='localhost';

ALTER USER mohit@localhost IDENTIFIED WITH 'test_plugin_server' AS 'haha';

--echo # Must have default password reuse interval
SHOW CREATE USER mohit@localhost;

--echo # Must be 0: password history on
SELECT COUNT(*) FROM mysql.password_history WHERE
  User='mohit' AND Host='localhost';

DROP USER mohit@localhost;

--echo # End of 8.0 tests

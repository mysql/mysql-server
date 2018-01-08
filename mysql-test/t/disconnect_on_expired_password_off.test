
## WL6587 Test --disconnect-on-expired-password=off

SHOW VARIABLES LIKE 'disconnect_on_expired_password';

--echo ## Test mysql client in non-interactive mode

CREATE USER 'bernt';
ALTER USER 'bernt' IDENTIFIED BY 'secret';

ALTER USER 'bernt' PASSWORD EXPIRE;
--echo # Should connect, but doing something should fail
--error 1
--exec $MYSQL -ubernt -psecret -e "SELECT 1" 2>&1
--echo # Login and set password should succeed
--exec $MYSQL -ubernt -psecret -e "ALTER USER 'bernt' IDENTIFIED BY 'secret';" 2>&1

DROP USER 'bernt';

--echo ## Test mysqltest login

CREATE USER 'bernt';
ALTER USER 'bernt' IDENTIFIED BY 'secret';
ALTER USER 'bernt' PASSWORD EXPIRE;
--echo # Login with mysqltest should work
connect(con2,localhost,bernt,secret,,);
--echo # But doing something should fail
--error 1820
SELECT 1;
disconnect con2;
connection default;
DROP USER 'bernt';

--echo ## Test mysqladmin

CREATE USER 'bernt';
ALTER USER 'bernt' IDENTIFIED BY 'secret';
GRANT ALL ON *.* TO 'bernt' WITH GRANT OPTION;
ALTER USER 'bernt' PASSWORD EXPIRE;

--echo # Doing something should connect but fail
--replace_result $MYSQLADMIN MYSQLADMIN
--error 1
--exec $MYSQLADMIN --no-defaults --user=bernt --password=secret -S $MASTER_MYSOCK -P $MASTER_MYPORT reload 2>&1
--echo # Setting password should succeed
--exec $MYSQLADMIN --no-defaults --user=bernt --password=secret -S $MASTER_MYSOCK -P $MASTER_MYPORT password newsecret --ssl-mode=REQUIRED 2>&1

DROP USER 'bernt';

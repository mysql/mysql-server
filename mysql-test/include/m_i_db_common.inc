connect (conn_root,$adminhost,$adminuser,$passwd,,,,$sslstate);
if ($passwd != ""){
  --error 1820
  SELECT user FROM mysql.user;
  set password='abc';
}
# Below two statement should show 2 (root + sys schema user)
SELECT user,host,plugin FROM mysql.user WHERE password_expired='n' ORDER BY user;
SELECT count(*) FROM mysql.user;
CREATE USER 'u1'@'localhost';
GRANT ALL ON *.* TO 'u1'@'localhost';

show databases;

# Reset database password to blank
set password='';

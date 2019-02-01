CREATE TABLE mysql.tmp_backup_tables_priv AS SELECT * FROM mysql.tables_priv;
CREATE TABLE mysql.tmp_backup_user AS SELECT * FROM mysql.user;
DROP DATABASE sys;
SHOW DATABASES;
Database
information_schema
mtr
mysql
performance_schema
test
# restart:--upgrade=FORCE
SHOW DATABASES;
Database
information_schema
mtr
mysql
performance_schema
sys
test
TRUNCATE TABLE mysql.tables_priv;
INSERT INTO mysql.tables_priv (SELECT * FROM mysql.tmp_backup_tables_priv);
DROP TABLE mysql.tmp_backup_tables_priv;
TRUNCATE TABLE mysql.user;
INSERT INTO mysql.user (SELECT * FROM mysql.tmp_backup_user);
DROP TABLE mysql.tmp_backup_user;
FLUSH PRIVILEGES;

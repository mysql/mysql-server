--disable_query_log
CREATE USER 'x_root'@'localhost' IDENTIFIED WITH 'mysql_native_password';
GRANT ALL ON *.* TO 'x_root'@'localhost' WITH GRANT OPTION;
--enable_query_log

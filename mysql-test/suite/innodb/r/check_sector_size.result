SET @@global.keyring_file_data="MYSQL_TMP_DIR/mysecret_keyring10";
SHOW VARIABLES LIKE 'innodb_flush_method';
Variable_name	Value
innodb_flush_method	O_DIRECT
CREATE TABLE t1(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;
# restart:--early-plugin-load=keyring_file=keyring_file.so --loose-keyring_file_data=MYSQL_TMP_DIR/mysecret_keyring10 --plugin-dir=KEYRING_PLUGIN_PATH
SELECT * FROM t1 LIMIT 10;
c1	c2
DROP TABLE t1;
# restart:

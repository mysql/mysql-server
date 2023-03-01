
--echo #
--echo # Bug#12688860 : SECURITY RECOMMENDATION: PASSWORDS ON CLI
--echo #

CREATE DATABASE b12688860_db;
CREATE TABLE b12688860_db.b12688860_tab (c1 INT);

--echo # Creating a temp sql file to be loaded.
--write_file $MYSQLTEST_VARDIR/tmp/b12688860_tab.sql
1
2
3
EOF

--exec $MYSQL_IMPORT -uroot --password="" b12688860_db $MYSQLTEST_VARDIR/tmp/b12688860_tab.sql 2>&1

SELECT * FROM b12688860_db.b12688860_tab;
DROP TABLE b12688860_db.b12688860_tab;
DROP DATABASE b12688860_db;

--echo # Dropping the temp file
--remove_file $MYSQLTEST_VARDIR/tmp/b12688860_tab.sql

--echo #
--echo # WL#2284: Increase the length of a user name
--echo #

CREATE USER 'user_with_length_32_abcdefghijkl'@'localhost';
GRANT ALL ON *.* TO 'user_with_length_32_abcdefghijkl'@'localhost';

--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --user=user_with_length_32_abcdefghijkl --protocol=TCP -e "create table mysql.test(a int)"
--exec echo "" > $MYSQL_TMP_DIR/test.txt
--exec $MYSQL_IMPORT --host=127.0.0.1 -P $MASTER_MYPORT --user=user_with_length_32_abcdefghijkl --protocol=TCP --local mysql $MYSQL_TMP_DIR/test.txt 

--echo # cleanup
DROP TABLE mysql.test;
DROP USER 'user_with_length_32_abcdefghijkl'@'localhost';
--remove_file $MYSQL_TMP_DIR/test.txt

--echo #
--echo # WL#13292: Deprecate legacy connection compression parameters
--echo #

CREATE TABLE mysql.test(a INT PRIMARY KEY);
--exec echo "" > $MYSQL_TMP_DIR/test.txt
--echo # exec mysqlimport --compress: must have a deprecation warning
--exec $MYSQL_IMPORT --host=127.0.0.1 -P $MASTER_MYPORT --protocol=TCP --compress --local mysql $MYSQL_TMP_DIR/test.txt 2>&1
--echo # exec mysqlimport --compression-algorithms: must not have a deprecation warning
--exec $MYSQL_IMPORT --host=127.0.0.1 -P $MASTER_MYPORT --protocol=TCP --compression-algorithms=zlib,uncompressed --local mysql $MYSQL_TMP_DIR/test.txt 2>&1

--echo # cleanup
DROP TABLE mysql.test;
--remove_file $MYSQL_TMP_DIR/test.txt

--echo #
--echo # Bug#34999015: mysqlimport doesn't escape reserved-word table names
--echo #   when used with --delete
--echo #

CREATE DATABASE b34999015_db;
CREATE TABLE b34999015_db.`KEY` (`ID` INT NOT NULL, PRIMARY KEY (`ID`));

--echo # Creating a temp sql file to be loaded.
--write_file $MYSQLTEST_VARDIR/tmp/KEY.txt
1
2
3
EOF

--echo # Test: should succeed
--exec $MYSQL_IMPORT -uroot b34999015_db --local --delete $MYSQLTEST_VARDIR/tmp/KEY.txt 2>&1

echo # cleanup
DROP TABLE b34999015_db.`KEY`;
DROP DATABASE b34999015_db;
--remove_file $MYSQLTEST_VARDIR/tmp/KEY.txt


--echo
--echo End of tests

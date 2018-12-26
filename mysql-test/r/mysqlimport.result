#
# Bug#12688860 : SECURITY RECOMMENDATION: PASSWORDS ON CLI
#
CREATE DATABASE b12688860_db;
CREATE TABLE b12688860_db.b12688860_tab (c1 INT);
# Creating a temp sql file to be loaded.
mysqlimport: [Warning] Using a password on the command line interface can be insecure.
b12688860_db.b12688860_tab: Records: 3  Deleted: 0  Skipped: 0  Warnings: 0
SELECT * FROM b12688860_db.b12688860_tab;
c1
1
2
3
DROP TABLE b12688860_db.b12688860_tab;
DROP DATABASE b12688860_db;
# Dropping the temp file
#
# WL#2284: Increase the length of a user name
#
CREATE USER 'user_with_length_32_abcdefghijkl'@'localhost';
GRANT ALL ON *.* TO 'user_with_length_32_abcdefghijkl'@'localhost';
mysql.test: Records: 1  Deleted: 0  Skipped: 0  Warnings: 1
# cleanup
DROP TABLE mysql.test;
DROP USER 'user_with_length_32_abcdefghijkl'@'localhost';

End of tests

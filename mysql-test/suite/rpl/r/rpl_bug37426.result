include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE char128_utf8 (i1 INT NOT NULL, c CHAR(128) CHARACTER SET utf8 NOT NULL, i2 INT NOT NULL);
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
INSERT INTO char128_utf8 VALUES ( 1, "123", 1 );
SELECT * FROM char128_utf8;
i1	c	i2
1	123	1
include/sync_slave_sql_with_master.inc
SELECT * FROM char128_utf8;
i1	c	i2
1	123	1
DROP TABLE char128_utf8;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc

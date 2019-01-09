DROP TABLE IF EXISTS t1;
==== Test BUG#32407 ====
select * from t1;
a
1
1
==== Test BINLOG statement w/o FD event ====
BINLOG '
SVtYRxMBAAAAKQAAADQBAAAAABAAAAAAAAAABHRlc3QAAnQxAAEDAAE=
SVtYRxcBAAAAIgAAAFYBAAAQABAAAAAAAAEAAf/+AgAAAA==
';
ERROR HY000: The BINLOG statement of type `Table_map` was not preceded by a format description BINLOG statement.
select * from t1;
a
1
1
==== Test BINLOG statement with FD event ====
BINLOG '
ODdYRw8BAAAAZgAAAGoAAAABAAQANS4xLjIzLXJjLWRlYnVnLWxvZwAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAA4N1hHEzgNAAgAEgAEBAQEEgAAUwAEGggAAAAICAgC
';
BINLOG '
TFtYRxMBAAAAKQAAAH8BAAAAABAAAAAAAAAABHRlc3QAAnQxAAEDAAE=
TFtYRxcBAAAAIgAAAKEBAAAQABAAAAAAAAEAAf/+AwAAAA==
';
select * from t1;
a
1
1
3
==== Test --base64-output=never on a binlog with row events ====
/*!50530 SET @@SESSION.PSEUDO_SLAVE_MODE=1*/;
/*!50003 SET @OLD_COMPLETION_TYPE=@@COMPLETION_TYPE,COMPLETION_TYPE=0*/;
DELIMITER /*!*/;
# at 4
<#>
ROLLBACK/*!*/;
# at 120
<#>
use `test`/*!*/;
SET TIMESTAMP=1360247561/*!*/;
<#>
SET @@session.foreign_key_checks=1, @@session.sql_auto_is_null=0, @@session.unique_checks=1, @@session.autocommit=1/*!*/;
SET @@session.sql_mode=1073741824/*!*/;
SET @@session.auto_increment_increment=1, @@session.auto_increment_offset=1/*!*/;
/*!\C utf8 *//*!*/;
SET @@session.character_set_client=33,@@session.collation_connection=33,@@session.collation_server=8/*!*/;
SET @@session.lc_time_names=0/*!*/;
SET @@session.collation_database=DEFAULT/*!*/;
CREATE TABLE t1 (a int) engine = myisam
/*!*/;
# at 233
<#>
SET TIMESTAMP=1360247563/*!*/;
BEGIN
/*!*/;
# at 305
<#>
ROLLBACK /* added by mysqlbinlog */ /*!*/;
SET @@SESSION.GTID_NEXT= 'AUTOMATIC' /* added by mysqlbinlog */ /*!*/;
DELIMITER ;
# End of log file
/*!50003 SET COMPLETION_TYPE=@OLD_COMPLETION_TYPE*/;
/*!50530 SET @@SESSION.PSEUDO_SLAVE_MODE=0*/;
==== Test non-matching FD event and Row event ====
BINLOG '
4CdYRw8BAAAAYgAAAGYAAAAAAAQANS4xLjE1LW5kYi02LjEuMjQtZGVidWctbG9nAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAADgJ1hHEzgNAAgAEgAEBAQEEgAATwAEGggICAg=
';
BINLOG '
Dl1YRxMBAAAAKQAAADQBAAAAABAAAAAAAAAABHRlc3QAAnQxAAEDAAE=
Dl1YRxcBAAAAIgAAAFYBAAAQABAAAAAAAAEAAf/+BQAAAA==
';
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use
select * from t1;
a
1
1
3
CREATE TABLE char128_utf8 (
i1 INT NOT NULL,
c CHAR(128) CHARACTER SET utf8 NOT NULL,
i2 INT NOT NULL);
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
CREATE TABLE char63_utf8 (
i1 INT NOT NULL,
c CHAR(63) CHARACTER SET utf8 NOT NULL,
i2 INT NOT NULL);
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
BINLOG '
MuNkSA8BAAAAZgAAAGoAAAAAAAQANS4xLjI1LXJjLWRlYnVnLWxvZwAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAAy42RIEzgNAAgAEgAEBAQEEgAAUwAEGggAAAAICAgC
';
BINLOG '
3u9kSBMBAAAANgAAAJYBAAAAABAAAAAAAAAABHRlc3QAC2NoYXI2M191dGY4AAMD/gMC/r0A
3u9kSBcBAAAAKgAAAMABAAAQABAAAAAAAAEAA//4AQAAAAMxMjMBAAAA
';
SELECT * FROM char63_utf8;
i1	c	i2
1	123	1
BINLOG '
iONkSBMBAAAANwAAAJkBAAAAABAAAAAAAAAABHRlc3QADGNoYXIxMjhfdXRmOAADA/4DAv6AAA==
iONkSBcBAAAAKwAAAMQBAAAQABAAAAAAAAEAA//4AQAAAAMAMTIzAQAAAA==
';
ERROR HY000: master may suffer from http://bugs.mysql.com/bug.php?id=37426 so slave stops; check error log on slave for more info
drop table t1, char63_utf8, char128_utf8;
call mtr.add_suppression("Slave SQL.*master suffers from this bug: http:..bugs.mysql.com.bug.php.id=37426.* Error_code: MY-013127");
call mtr.add_suppression("Slave SQL.*Table definition on master and slave does not match: Column 1 size mismatch.* Error_code: 1535");
call mtr.add_suppression("Slave SQL.*Column 1 of table .test.char128_utf8. cannot be converted.* Error_code: MY-013146");
#
# Bug #54393: crash and/or valgrind errors in
# mysql_client_binlog_statement
#
BINLOG '';
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use
BINLOG '123';
ERROR HY000: Decoding of base64 string failed
BINLOG '-2079193929';
ERROR HY000: Decoding of base64 string failed
BINLOG 'xç↓%~∙D╒ƒ╡';
ERROR HY000: Decoding of base64 string failed

CREATE TABLE t1 (a INT);
#
# Verify that inserting gtid into mysql.gtid_executed table
# explicitly by a XA transaction causes an error.
#
XA START '1';
INSERT INTO t1 VALUES(1);
INSERT INTO mysql.gtid_executed VALUES("aaaaaaaa-aaaa-aaaa-aaaa-bug#21452916", 1, 1);
ERROR HY000: Please do not modify the gtid_executed table with an XA transaction. This is an internal system table used to store GTIDs for committed transactions. Although modifying it can lead to an inconsistent GTID state, if necessary you can modify it with a non-XA transaction.
XA END '1';
XA PREPARE '1';
XA COMMIT '1';
FLUSH LOGS;
#
# Verify that users can not insert gtid into mysql.gtid_executed
# table explicitly by a XA transaction.
#
include/assert.inc [Table mysql.gtid_executed must not contain row with source_uuid=aaaaaaaa-aaaa-aaaa-aaaa-bug#21452916]
#
# Verify that updating mysql.gtid_executed table
# explicitly by a XA transaction causes an error.
#
XA START '1';
INSERT INTO t1 VALUES(2);
UPDATE mysql.gtid_executed SET source_uuid = "aaaaaaaa-aaaa-aaaa-aaaa-bug#21452916" WHERE source_uuid = '$master_uuid';
ERROR HY000: Please do not modify the gtid_executed table with an XA transaction. This is an internal system table used to store GTIDs for committed transactions. Although modifying it can lead to an inconsistent GTID state, if necessary you can modify it with a non-XA transaction.
XA END '1';
XA PREPARE '1';
XA COMMIT '1';
#
# Verify that users can not update mysql.gtid_executed
# table explicitly by a XA transaction.
#
include/assert.inc [Table mysql.gtid_executed must not contain row with source_uuid=aaaaaaaa-aaaa-aaaa-aaaa-bug#21452916]
#
# Verify that deleting from mysql.gtid_executed table
# explicitly by a XA transaction causes an error.
#
XA START '1';
INSERT INTO t1 VALUES(3);
DELETE FROM mysql.gtid_executed WHERE source_uuid = '$master_uuid';
ERROR HY000: Please do not modify the gtid_executed table with an XA transaction. This is an internal system table used to store GTIDs for committed transactions. Although modifying it can lead to an inconsistent GTID state, if necessary you can modify it with a non-XA transaction.
XA END '1';
XA PREPARE '1';
XA COMMIT '1';
#
# Verify that users can not delete from mysql.gtid_executed
# table explicitly by a XA transaction.
#
include/assert.inc [Table mysql.gtid_executed must contain GTID(s) with source_uuid = master_uuid]
DROP TABLE t1;

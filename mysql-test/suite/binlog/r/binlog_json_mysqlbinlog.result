==== Generate binlog ====
RESET MASTER;
CREATE TABLE t (i INT, j JSON);
SET @@SESSION.BINLOG_ROW_IMAGE = MINIMAL;
INSERT INTO t VALUES (1, '{"a" : 1}');
UPDATE t SET j = '1' WHERE i = 1;
DELETE FROM t;
DROP TABLE t;
==== Check output from mysqlbinlog ====
include/save_binlog_position.inc
include/mysqlbinlog.inc
### INSERT INTO `test`.`t`
### SET
###   @1=1 /* INT meta=0 nullable=1 is_null=0 */
###   @2='{"a": 1}' /* JSON meta=4 nullable=1 is_null=0 */
### UPDATE `test`.`t`
### WHERE
###   @1=1 /* INT meta=0 nullable=1 is_null=0 */
###   @2='{"a": 1}' /* JSON meta=4 nullable=1 is_null=0 */
### SET
###   @2='LONG_JSON
### UPDATE `test`.`t`
### WHERE
###   @1=1 /* INT meta=0 nullable=1 is_null=0 */
###   @2='LONG_JSON
### SET
###   @2='1' /* JSON meta=4 nullable=1 is_null=0 */
### DELETE FROM `test`.`t`
### WHERE
###   @1=1 /* INT meta=0 nullable=1 is_null=0 */
###   @2='1' /* JSON meta=4 nullable=1 is_null=0 */

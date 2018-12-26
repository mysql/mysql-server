include/rpl_set_gtid_mode.inc [OFF on servers 1]
[connection default]
# 1) start with GTID_NEXT to 'AUTOMATIC' and
#    and then change to 'ANONYMOUS'
#
# 1.a) Create transaction, create temporary
#      table, end transaction with XA PREPARE
#      and XA COMMIT
#
SET @@SESSION.GTID_NEXT = 'AUTOMATIC';
XA START 'trx1';
CREATE TEMPORARY TABLE temp_table_1 (a INT);
XA END 'trx1';
XA PREPARE 'trx1';
SET @@SESSION.GTID_NEXT = 'ANONYMOUS';
[connection verify]
include/assert.inc [There are no AUTOMATIC violating transactions]
include/assert.inc [There are no ANONYMOUS violating transactions]
[connection default]
XA COMMIT 'trx1';
DROP TEMPORARY TABLE IF EXISTS temp_table_1;
# 1.b) Create transaction, create temporary
#      table, end transaction with XA PREPARE
#      and XA ROLLBACK
#
SET @@SESSION.GTID_NEXT = 'AUTOMATIC';
XA START 'trx1';
CREATE TEMPORARY TABLE temp_table_1 (a INT);
XA END 'trx1';
XA PREPARE 'trx1';
SET @@SESSION.GTID_NEXT = 'ANONYMOUS';
[connection verify]
include/assert.inc [There are no AUTOMATIC violating transactions]
include/assert.inc [There are no ANONYMOUS violating transactions]
[connection default]
XA ROLLBACK 'trx1';
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
DROP TEMPORARY TABLE IF EXISTS temp_table_1;
# 1.c) Create transaction, create temporary
#      table, end transaction with XA COMMIT
#      ONE PHASE
#
SET @@SESSION.GTID_NEXT = 'AUTOMATIC';
XA START 'trx1';
CREATE TEMPORARY TABLE temp_table_1 (a INT);
XA END 'trx1';
XA COMMIT 'trx1' ONE PHASE;
SET @@SESSION.GTID_NEXT = 'ANONYMOUS';
[connection verify]
include/assert.inc [There are no AUTOMATIC violating transactions]
include/assert.inc [There are no ANONYMOUS violating transactions]
[connection default]
DROP TEMPORARY TABLE IF EXISTS temp_table_1;
[connection default]
# 2) start with GTID_NEXT to 'ANONYMOUS' and
#    and then change to 'AUTOMATIC'
#
# 2.a) Create transaction, create temporary
#      table, end transaction with XA PREPARE
#      and XA COMMIT
#
SET @@SESSION.GTID_NEXT = 'ANONYMOUS';
XA START 'trx1';
CREATE TEMPORARY TABLE temp_table_1 (a INT);
XA END 'trx1';
XA PREPARE 'trx1';
SET @@SESSION.GTID_NEXT = 'AUTOMATIC';
[connection verify]
include/assert.inc [There are no AUTOMATIC violating transactions]
include/assert.inc [There are no ANONYMOUS violating transactions]
[connection default]
XA COMMIT 'trx1';
DROP TEMPORARY TABLE IF EXISTS temp_table_1;
# 2.b) Create transaction, create temporary
#      table, end transaction with XA PREPARE
#      and XA ROLLBACK
#
SET @@SESSION.GTID_NEXT = 'ANONYMOUS';
XA START 'trx1';
CREATE TEMPORARY TABLE temp_table_1 (a INT);
XA END 'trx1';
XA PREPARE 'trx1';
SET @@SESSION.GTID_NEXT = 'AUTOMATIC';
[connection verify]
include/assert.inc [There are no AUTOMATIC violating transactions]
include/assert.inc [There are no ANONYMOUS violating transactions]
[connection default]
XA ROLLBACK 'trx1';
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
DROP TEMPORARY TABLE IF EXISTS temp_table_1;
# 2.c) Create transaction, create temporary
#      table, end transaction with XA COMMIT
#      ONE PHASE
#
SET @@SESSION.GTID_NEXT = 'ANONYMOUS';
XA START 'trx1';
CREATE TEMPORARY TABLE temp_table_1 (a INT);
XA END 'trx1';
XA COMMIT 'trx1' ONE PHASE;
SET @@SESSION.GTID_NEXT = 'AUTOMATIC';
[connection verify]
include/assert.inc [There are no AUTOMATIC violating transactions]
include/assert.inc [There are no ANONYMOUS violating transactions]
[connection default]
DROP TEMPORARY TABLE IF EXISTS temp_table_1;

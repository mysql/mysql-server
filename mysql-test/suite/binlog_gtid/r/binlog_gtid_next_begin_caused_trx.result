RESET MASTER;
SET GTID_NEXT= 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
CREATE TABLE t1 (c1 INT);
SET GTID_NEXT= 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2';
BEGIN;
#
# Check that the command 'BEGIN' causes an error
# 'ER_CANT_DO_IMPLICIT_COMMIT_IN_TRX_WHEN_GTID_NEXT_IS_SET' inside
# an empty transaction, since it causes an implicit commit. And we
# do not save the gtid specified by GTID_NEXT into
# GLOBAL@gtid_executed in the case.
#
BEGIN;
ERROR HY000: Cannot execute statements with implicit commit inside a transaction when @@SESSION.GTID_NEXT == 'UUID:NUMBER'.
include/assert.inc [Dose not commit gtid aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2]
INSERT INTO t1 VALUES (1);
#
# Check that the command 'BEGIN' causes an error
# 'ER_CANT_DO_IMPLICIT_COMMIT_IN_TRX_WHEN_GTID_NEXT_IS_SET' inside
# a non-empty transaction, since it causes an implicit commit. And
# we do not save the gtid specified by GTID_NEXT into
# GLOBAL@gtid_executed in the case.
#
BEGIN;
ERROR HY000: Cannot execute statements with implicit commit inside a transaction when @@SESSION.GTID_NEXT == 'UUID:NUMBER'.
include/assert.inc [Dose not commit gtid aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2]
#
# Check that we save the gtid specified by GTID_NEXT into
# GLOBAL@gtid_executed when committing the transaction.
#
COMMIT;
include/assert.inc [Committed gtid aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2]
SET GTID_NEXT= 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:3';
DROP TABLE t1;

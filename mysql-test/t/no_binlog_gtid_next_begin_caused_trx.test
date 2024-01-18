# ==== Purpose ====
#
# When binlog is disabled and @@SESSION.GTID_NEXT == 'UUID:NUMBER',
# verify that the command 'BEGIN' causes an error
# 'ER_CANT_DO_IMPLICIT_COMMIT_IN_TRX_WHEN_GTID_NEXT_IS_SET' inside
# an empty/a non-empty transaction, since it causes an implicit
# commit. We do not save the gtid specified by GTID_NEXT into
# GLOBAL@gtid_executed in the case.
#
# ==== Implementation ====
#
# See common/binlog/gtid_next_begin_caused_trx.test
#
# ==== References ====
#
# Bug#22130929  GTID_NEXT AND BEGIN BEHAVIOR IS DIFFERENT B/W BINLOG AND BINLOG-LESS SERVER
#


# Should be tested against "binlog disabled" server
--source include/not_log_bin.inc

# Make sure the test is repeatable
RESET BINARY LOGS AND GTIDS;

--source common/binlog/gtid_next_begin_caused_trx.test

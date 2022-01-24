set @save_binlog_checksum= @@global.binlog_checksum;
set @@global.binlog_checksum = default;
select @@global.binlog_checksum as 'must be CRC32 by default';
must be CRC32 by default
CRC32
select @@session.binlog_checksum as 'no session var';
ERROR HY000: Variable 'binlog_checksum' is a GLOBAL variable
set @@global.binlog_checksum = CRC32;
set @@global.binlog_checksum = CRC32;
set @@global.source_verify_checksum = 0;
set @@global.source_verify_checksum = default;
set @@global.binlog_checksum = ADLER32;
ERROR 42000: Variable 'binlog_checksum' can't be set to the value of 'ADLER32'
#
# Bug#22914463 SET BINLOG_CHECKSUM CRASHES WHEN EXECUTED INSIDE
#              TRANSACTION
#
# Check that changing binlog_checksum inside transacation
# is not allowed.
#
BEGIN;
SET @@global.binlog_checksum = CRC32;
ERROR HY000: The system variable binlog_checksum cannot be set when there is an ongoing transaction.
COMMIT;
XA START 'trx1';
SET @@global.binlog_checksum = CRC32;
ERROR HY000: The system variable binlog_checksum cannot be set when there is an ongoing transaction.
XA END 'trx1';
XA ROLLBACK 'trx1';
#
# Check that changing binlog_checksum if the session owns
# a GTID is not allowed.
#
SET GLOBAL GTID_MODE=OFF_PERMISSIVE;
SET GLOBAL GTID_MODE=ON_PERMISSIVE;
SET GLOBAL ENFORCE_GTID_CONSISTENCY=ON;
SET GLOBAL GTID_MODE=ON;
SET SESSION GTID_NEXT='11111111-AAAA-2222-BBBB-000000000000:5';
SET @@global.binlog_checksum = CRC32;
ERROR HY000: Variable binlog_checksum cannot be changed by a client that owns a GTID. The client owns 11111111-aaaa-2222-bbbb-000000000000:5. Ownership is released on COMMIT or ROLLBACK.
SET SESSION GTID_NEXT='AUTOMATIC';
SET GLOBAL GTID_MODE=ON_PERMISSIVE;
SET GLOBAL GTID_MODE=OFF_PERMISSIVE;
SET GLOBAL ENFORCE_GTID_CONSISTENCY=OFF;
SET GLOBAL GTID_MODE=OFF;
set @@global.binlog_checksum = @save_binlog_checksum;

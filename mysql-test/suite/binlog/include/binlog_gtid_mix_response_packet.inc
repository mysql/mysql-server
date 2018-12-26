#
# WL#6972: Collect GTIDs to include in the protocol's OK packet

# The test goes like this:
# 1. Statements are executed and depending whether they are
#    updates or not, they get GTID assigned
# 2. After each statement, we track the contents of the return packet.
#    They are saved to the result file.
#
#
# Scenarios tested:
# 1. DDL
# 2. Explicit RW transaction
# 3. Implicit RW transaction commit
# 4. RO transaction following an RW autocommit transaction
# 5. Finally DDL again
#
# These scenarios are tested for all three values of the
# variable SESSION_TRACK_GTIDS
#

--let $uuid= `SELECT @@server_uuid`
--let $i=3

--enable_session_track_info

while($i)
{
  if ($i == 3)
  {
    --let $_option= ALL_GTIDS
  }
  if ($i == 2)
  {
    --let $_option= OWN_GTID
  }
  if ($i == 1)
  {
    --let $_option= OFF
  }

  --eval SET SESSION SESSION_TRACK_GTIDS=$_option
  RESET MASTER;

  # transaction 1 - $uuid:1
  CREATE TABLE t1 (c1 INT) Engine=InnoDB;

  # transaction 2 - $uuid:2
  BEGIN;
  INSERT INTO t1 VALUES (1);
  COMMIT;

  # transaction $uuid:3 and $uuid:4
  BEGIN;
  INSERT INTO t1 VALUES (2);
  CREATE TABLE t2 (c1 INT) Engine=InnoDB;

  # Testing RO transaction following a RW transaction

  # transaction $uuid:5
  INSERT INTO t2 VALUES (1);
  # RO transaction
  SELECT * FROM t1;

  BEGIN;
  INSERT INTO t1 VALUES (2);
  SAVEPOINT s;
  INSERT INTO t1 VALUES (3);
  ROLLBACK TO SAVEPOINT s;
  ROLLBACK;

  # transaction $uuid:6
  DROP TABLE t1, t2;

  --dec $i
}
--disable_session_track_info

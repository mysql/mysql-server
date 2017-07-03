RESET MASTER;
SET GTID_NEXT = 'ANONYMOUS';
CREATE TABLE t1 (a int) ENGINE=MyISAM;
SET GTID_NEXT = 'ANONYMOUS';
CREATE TABLE t2 (a int) ENGINE=InnoDB;
SET GTID_NEXT = 'ANONYMOUS';
#
# Make DROP TABLE statement to fail partially, by injecting error
# after dropping MyISAM table.
#
SET @@debug="+d,rm_table_no_locks_abort_before_atomic_tables";
DROP TABLE t1, t2;
ERROR HY000: Unknown error
SET @@debug="-d,rm_table_no_locks_abort_before_atomic_tables";
#
# The table t1 was dropped, which means DROP TABLE
# can be failed partially.
#
SHOW CREATE TABLE t1;
ERROR 42S02: Table 'test.t1' doesn't exist
#
# When binlog is disabled, verify that we can set
# GTID_NEXT to 'ANONYMOUS' after the partially failed
# DROP TABLE statement consumed its 'ANONYMOUS' gtid.
#
SET GTID_NEXT = 'ANONYMOUS';
CREATE USER u1@h;
SET GTID_NEXT = 'ANONYMOUS';
DROP USER u1@h, u2@h;
ERROR HY000: Operation DROP USER failed for 'u2'@'h'
#
# When binlog is disabled, verify that we can set
# GTID_NEXT to 'ANONYMOUS' after the failed DROP USER
# statement  did not consumed its 'ANONYMOUS' gtid.
#
SET GTID_NEXT = 'ANONYMOUS';
DROP USER u1@h;
SET GTID_NEXT = 'ANONYMOUS';
DROP TABLE t2;

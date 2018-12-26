include/save_binlog_position.inc
# -----------------------------------------------------------------------
# Begin : Tests for ALTER USER
CREATE USER userX, userY, userZ;
include/save_binlog_position.inc
ALTER USER IF EXISTS userX, userY, userZ,
userW IDENTIFIED WITH 'mysql_native_password' BY 'abcd'
                    ACCOUNT LOCK;
Warnings:
Note	3162	Authorization ID 'userW'@'%' does not exist.
# This event sequence pattern MUST be present in binlog: !Q(ALTER USER.*userX.*userY.*userZ.*userW.*ACCOUNT LOCK.*)
include/assert_binlog_events.inc
include/save_binlog_position.inc
DROP USER userX, userY, userZ;
# End : Tests for ALTER USER IF EXISTS
# -----------------------------------------------------------------------

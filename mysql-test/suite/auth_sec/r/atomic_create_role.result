include/save_binlog_position.inc
# ----------------------------------------------------------------------
# Begin : Tests for CREATE ROLE
CREATE USER userX;
CREATE ROLE roleX;
include/save_binlog_position.inc
# Case 1 : Execute CREATE ROLE for existing users
CREATE ROLE userZ, roleZ, userX, roleX;
ERROR HY000: Operation CREATE ROLE failed for 'userX'@'%','roleX'@'%'
# Must not show entries for userZ and roleZ
SELECT user FROM mysql.user WHERE user LIKE 'user%';
user
userX
SELECT user FROM mysql.user WHERE user LIKE 'role%';
user
roleX
# This event sequence pattern MUST NOT be present in binlog: !Q(CREATE ROLE.*userZ.*roleZ.*userX.*roleX.*)
include/assert_binlog_events.inc
include/save_binlog_position.inc
# Case 2 : Execute CREATE ROLE for non-existing users
CREATE ROLE roleZ, roleA;
# Must show entry for roleZ, roleA
SELECT user FROM mysql.user WHERE user LIKE 'role%';
user
roleA
roleX
roleZ
# This event sequence pattern MUST be present in binlog: !Q(CREATE ROLE.*roleZ.*roleA.*)
include/assert_binlog_events.inc
DROP USER userX;
DROP ROLE roleX, roleZ, roleA;
include/save_binlog_position.inc
# End : Tests for CREATE ROLE
# ----------------------------------------------------------------------
# Begin : Tests for CREATE ROLE IF NOT EXISTS
CREATE ROLE roleX;
include/save_binlog_position.inc
# Case 1 : Execute CREATE ROLE IF NOT EXISTS for exising and non-existing users
CREATE ROLE IF NOT EXISTS roleX, roleY, roleZ;
Warnings:
Note	3163	Authorization ID 'roleX'@'%' already exists.
# Must show entry for roleY and roleZ
SELECT user FROM mysql.user WHERE user LIKE 'role%';
user
roleX
roleY
roleZ
# This event sequence pattern MUST be present in binlog: !Q(CREATE ROLE IF NOT EXISTS.*roleX.*roleY.*roleZ.*)
include/assert_binlog_events.inc
include/save_binlog_position.inc
# case 2 : Execute CREATE ROLE IF NOT EXISTS for existing users
CREATE ROLE IF NOT EXISTS roleX, roleY, roleZ;
Warnings:
Note	3163	Authorization ID 'roleX'@'%' already exists.
Note	3163	Authorization ID 'roleY'@'%' already exists.
Note	3163	Authorization ID 'roleZ'@'%' already exists.
# This event sequence pattern MUST be present in binlog: !Q(CREATE ROLE IF NOT EXISTS.*roleX.*roleY.*roleZ.*)
include/assert_binlog_events.inc
DROP ROLE roleX, roleY, roleZ;
include/save_binlog_position.inc
# End : Tests for CREATE ROLE IF NOT EXISTS
# ----------------------------------------------------------------------

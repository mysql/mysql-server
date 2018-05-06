include/save_binlog_position.inc
# ----------------------------------------------------------------------
# Begin : Tests for GRANT : Routine grants
CREATE USER userX, userY, userZ;
CREATE DATABASE db1;
CREATE PROCEDURE db1.proc1()
BEGIN
SELECT 1234;
END ||
include/save_binlog_position.inc
# Case 1 : Valid grants
GRANT EXECUTE ON PROCEDURE db1.proc1 TO userX, userY, userZ;
# Must show new grant
SHOW GRANTS FOR userX;
Grants for userX@%
GRANT USAGE ON *.* TO `userX`@`%`
GRANT EXECUTE ON PROCEDURE `db1`.`proc1` TO `userX`@`%`
SHOW GRANTS FOR userY;
Grants for userY@%
GRANT USAGE ON *.* TO `userY`@`%`
GRANT EXECUTE ON PROCEDURE `db1`.`proc1` TO `userY`@`%`
SHOW GRANTS FOR userZ;
Grants for userZ@%
GRANT USAGE ON *.* TO `userZ`@`%`
GRANT EXECUTE ON PROCEDURE `db1`.`proc1` TO `userZ`@`%`
# This event sequence pattern MUST be present in binlog: !Q(GRANT EXECUTE ON PROCEDURE.*db1.*proc1.*TO.*userX.*userY.*userZ.*)
include/assert_binlog_events.inc
include/save_binlog_position.inc
# Case 6 : Create new user with an account without CREATE USER privilege
CREATE USER userA, userB;
GRANT EXECUTE ON PROCEDURE db1.proc1 to userX WITH GRANT OPTION;
SHOW GRANTS FOR userY;
Grants for userY@%
GRANT USAGE ON *.* TO `userY`@`%`
GRANT EXECUTE ON PROCEDURE `db1`.`proc1` TO `userY`@`%`
SHOW GRANTS FOR userZ;
Grants for userZ@%
GRANT USAGE ON *.* TO `userZ`@`%`
GRANT EXECUTE ON PROCEDURE `db1`.`proc1` TO `userZ`@`%`
SHOW GRANTS FOR userA;
Grants for userA@%
GRANT USAGE ON *.* TO `userA`@`%`
SHOW GRANTS FOR userB;
Grants for userB@%
GRANT USAGE ON *.* TO `userB`@`%`
include/save_binlog_position.inc
# should succeed
GRANT EXECUTE ON PROCEDURE db1.proc1 TO userY, userZ;
# should fail
GRANT EXECUTE ON PROCEDURE db1.proc1 TO userA, userB, userC;
ERROR 42000: You are not allowed to create a user with GRANT
# Must show updated grants
SHOW GRANTS FOR userY;
Grants for userY@%
GRANT USAGE ON *.* TO `userY`@`%`
GRANT EXECUTE ON PROCEDURE `db1`.`proc1` TO `userY`@`%`
SHOW GRANTS FOR userZ;
Grants for userZ@%
GRANT USAGE ON *.* TO `userZ`@`%`
GRANT EXECUTE ON PROCEDURE `db1`.`proc1` TO `userZ`@`%`
# Must not have any change
SHOW GRANTS FOR userA;
Grants for userA@%
GRANT USAGE ON *.* TO `userA`@`%`
SHOW GRANTS FOR userB;
Grants for userB@%
GRANT USAGE ON *.* TO `userB`@`%`
# This event sequence pattern MUST be present in binlog: !Q(GRANT EXECUTE ON PROCEDURE.*db1.*proc1.*TO.*userY.*userZ.*)
include/assert_binlog_events.inc
DROP USER userX, userY, userZ, userA, userB;
DROP PROCEDURE db1.proc1;
DROP DATABASE db1;
include/save_binlog_position.inc
# End : Tests for GRANT : Routine grants
# ----------------------------------------------------------------------

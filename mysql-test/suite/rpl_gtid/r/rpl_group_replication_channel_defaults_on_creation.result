#
# 1. Create a user so we can set a privileged user to create GR channels
CREATE USER 'u1'@'localhost';
#
# 2. Create a group replication applier channel
#    Verify SOURCE_AUTO_POSITION, REQUIRE_ROW_FORMAT and GTID_ONLY are enabled
CHANGE REPLICATION SOURCE TO PRIVILEGE_CHECKS_USER = 'u1'@'localhost' FOR CHANNEL "group_replication_applier";
include/assert.inc [Auto_position is set to 1 for GR applier channel]
include/assert.inc [Require_row_format is set to 1 for GR applier channel]
include/assert.inc [Gtid_only is set to 1 for GR applier channel]
#
# 3. For the group replication applier channel
#    Verify SOURCE_AUTO_POSITION, REQUIRE_ROW_FORMAT and GTID_ONLY can't be modified
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0 FOR CHANNEL "group_replication_applier";
ERROR HY000: CHANGE MASTER with the given parameters cannot be performed on channel 'group_replication_applier'.
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1 FOR CHANNEL "group_replication_applier";
ERROR HY000: CHANGE MASTER with the given parameters cannot be performed on channel 'group_replication_applier'.
CHANGE REPLICATION SOURCE TO REQUIRE_ROW_FORMAT = 0 FOR CHANNEL "group_replication_applier";
ERROR HY000: CHANGE MASTER with the given parameters cannot be performed on channel 'group_replication_applier'.
CHANGE REPLICATION SOURCE TO REQUIRE_ROW_FORMAT = 1 FOR CHANNEL "group_replication_applier";
ERROR HY000: CHANGE MASTER with the given parameters cannot be performed on channel 'group_replication_applier'.
CHANGE REPLICATION SOURCE TO GTID_ONLY = 0 FOR CHANNEL "group_replication_applier";
ERROR HY000: CHANGE MASTER with the given parameters cannot be performed on channel 'group_replication_applier'.
CHANGE REPLICATION SOURCE TO GTID_ONLY = 1 FOR CHANNEL "group_replication_applier";
ERROR HY000: CHANGE MASTER with the given parameters cannot be performed on channel 'group_replication_applier'.
include/assert.inc [Auto_position is set to 1 for GR applier channel]
include/assert.inc [Require_row_format is set to 1 for GR applier channel]
include/assert.inc [Gtid_only is set to 1 for GR applier channel]
#
# 4. Create a group replication recovery channel
#    Verify SOURCE_AUTO_POSITION, REQUIRE_ROW_FORMAT and GTID_ONLY are enabled
CHANGE REPLICATION SOURCE TO PRIVILEGE_CHECKS_USER = 'u1'@'localhost' FOR CHANNEL "group_replication_recovery";
include/assert.inc [Auto_position is set to 1 for GR recovery channel]
include/assert.inc [Require_row_format is set to 1 for GR recovery channel]
include/assert.inc [Gtid_only is set to 1 for GR recovery channel]
#
# 5. For the group replication recovery channel
#    Verify SOURCE_AUTO_POSITION, REQUIRE_ROW_FORMAT and GTID_ONLY can't be modified
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0 FOR CHANNEL "group_replication_recovery";
ERROR HY000: CHANGE MASTER with the given parameters cannot be performed on channel 'group_replication_recovery'.
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1 FOR CHANNEL "group_replication_recovery";
ERROR HY000: CHANGE MASTER with the given parameters cannot be performed on channel 'group_replication_recovery'.
CHANGE REPLICATION SOURCE TO REQUIRE_ROW_FORMAT = 0 FOR CHANNEL "group_replication_recovery";
ERROR HY000: CHANGE MASTER with the given parameters cannot be performed on channel 'group_replication_recovery'.
CHANGE REPLICATION SOURCE TO REQUIRE_ROW_FORMAT = 1 FOR CHANNEL "group_replication_recovery";
ERROR HY000: CHANGE MASTER with the given parameters cannot be performed on channel 'group_replication_recovery'.
CHANGE REPLICATION SOURCE TO GTID_ONLY = 0 FOR CHANNEL "group_replication_recovery";
ERROR HY000: CHANGE MASTER with the given parameters cannot be performed on channel 'group_replication_recovery'.
CHANGE REPLICATION SOURCE TO GTID_ONLY = 1 FOR CHANNEL "group_replication_recovery";
ERROR HY000: CHANGE MASTER with the given parameters cannot be performed on channel 'group_replication_recovery'.
include/assert.inc [Auto_position is set to 1 for GR recovery channel]
include/assert.inc [Require_row_format is set to 1 for GR recovery channel]
include/assert.inc [Gtid_only is set to 1 for GR recovery channel]
#
# 6. Delete the channels
#    Verify they cannot be created when GTID_MODE != ON
RESET REPLICA ALL;
SET GLOBAL GTID_MODE = ON_PERMISSIVE;
CHANGE REPLICATION SOURCE TO PRIVILEGE_CHECKS_USER = 'u1'@'localhost' FOR CHANNEL "group_replication_applier";
ERROR HY000: When configuring a group replication channel you must do it when GTID_MODE = ON.
CHANGE REPLICATION SOURCE TO PRIVILEGE_CHECKS_USER = 'u1'@'localhost' FOR CHANNEL "group_replication_recovery";
ERROR HY000: When configuring a group replication channel you must do it when GTID_MODE = ON.
#
# 7. Cleanup
SET GLOBAL GTID_MODE = ON;
DROP USER 'u1'@'localhost';

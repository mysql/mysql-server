include/assert.inc [Default GLOBAL value is FALSE]
SET @@GLOBAL.replication_optimize_for_static_plugin_config = OFF;
include/assert.inc [Reported value matches the one set (set OFF in GLOBAL scope)]
SET @@GLOBAL.replication_optimize_for_static_plugin_config = FALSE;
include/assert.inc [Reported value matches the one set (set FALSE in GLOBAL scope)]
SET @@GLOBAL.replication_optimize_for_static_plugin_config = 0;
include/assert.inc [Reported value matches the one set (set 0 in GLOBAL scope)]
SET @@GLOBAL.replication_optimize_for_static_plugin_config = ON;
include/assert.inc [Reported value matches the one set (set ON in GLOBAL scope)]
SET @@GLOBAL.replication_optimize_for_static_plugin_config = TRUE;
include/assert.inc [Reported value matches the one set (set TRUE in GLOBAL scope)]
SET @@GLOBAL.replication_optimize_for_static_plugin_config = 1;
include/assert.inc [Reported value matches the one set (set 1 in GLOBAL scope)]
CREATE USER user1;
SET GLOBAL replication_optimize_for_static_plugin_config = 1;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
GRANT SUPER ON *.* TO user1@'%';
Warnings:
Warning	1287	The SUPER privilege identifier is deprecated
FLUSH PRIVILEGES;
SET GLOBAL replication_optimize_for_static_plugin_config = 1;
DROP USER user1;
BEGIN;
SET GLOBAL replication_optimize_for_static_plugin_config = ON;
ROLLBACK;
SET GLOBAL replication_optimize_for_static_plugin_config = 1;
INSTALL PLUGIN rpl_semi_sync_source SONAME 'SEMISYNC_MASTER_LIB';
UNINSTALL PLUGIN rpl_semi_sync_source;
Warnings:
Warning	1620	Plugin is busy and will be uninstalled on shutdown
include/assert.inc [Semi-sync not disposed because it is in use]
SET GLOBAL replication_optimize_for_static_plugin_config = 0;
include/assert.inc [Semi-sync disposed because it is no longer in use]
INSTALL PLUGIN rpl_semi_sync_source SONAME 'SEMISYNC_MASTER_LIB';
UNINSTALL PLUGIN rpl_semi_sync_source;
include/assert.inc [Semi-sync disposed because it is no longer in use]
SET @@global.replication_optimize_for_static_plugin_config = 0;

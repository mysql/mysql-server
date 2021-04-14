include/assert.inc [Default GLOBAL value is FALSE]
SET PERSIST_ONLY skip_replica_start = ON;
# restart
include/assert.inc [Reported value matches the one set (set ON in PERSIST_ONLY scope)]
SET PERSIST_ONLY skip_replica_start = OFF;
# restart
include/assert.inc [Reported value matches the one set (set OFF in PERSIST_ONLY scope)]
SET PERSIST_ONLY skip_replica_start = TRUE;
# restart
include/assert.inc [Reported value matches the one set (set TRUE in PERSIST_ONLY scope)]
SET PERSIST_ONLY skip_replica_start = FALSE;
# restart
include/assert.inc [Reported value matches the one set (set FALSE in PERSIST_ONLY scope)]
SET PERSIST_ONLY skip_replica_start = 1;
# restart
include/assert.inc [Reported value matches the one set (set 1 in PERSIST_ONLY scope)]
SET PERSIST_ONLY skip_replica_start = 0;
# restart
include/assert.inc [Reported value matches the one set (set 0 in PERSIST_ONLY scope)]
RESET PERSIST skip_replica_start;
# restart: --skip-replica-start=ON
include/assert.inc [Reported value matches the one set (set ON in `--skip-replica-start` parameter)]
# restart: --skip-replica-start=OFF
include/assert.inc [Reported value matches the one set (set OFF in `--skip-replica-start` parameter)]
# restart: --skip-replica-start=TRUE
include/assert.inc [Reported value matches the one set (set TRUE in `--skip-replica-start` parameter)]
# restart: --skip-replica-start=FALSE
include/assert.inc [Reported value matches the one set (set FALSE in `--skip-replica-start` parameter)]
# restart: --skip-replica-start=1
include/assert.inc [Reported value matches the one set (set 1 in `--skip-replica-start` parameter)]
# restart: --skip-replica-start=0
include/assert.inc [Reported value matches the one set (set 0 in `--skip-replica-start` parameter)]
CREATE USER user1;
SET PERSIST_ONLY skip_replica_start = 1;
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
GRANT SYSTEM_VARIABLES_ADMIN, PERSIST_RO_VARIABLES_ADMIN ON *.* TO user1@'%';
FLUSH PRIVILEGES;
SET PERSIST_ONLY skip_replica_start = 1;
DROP USER user1;
RESET PERSIST skip_replica_start;
BEGIN;
SET PERSIST_ONLY skip_replica_start = ON;
ROLLBACK;
RESET PERSIST skip_replica_start;

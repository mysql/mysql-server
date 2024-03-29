include/assert.inc [Default GLOBAL value is FALSE]
SET @@GLOBAL.replication_sender_observe_commit_only = OFF;
include/assert.inc [Reported value matches the one set (set OFF in GLOBAL scope)]
SET @@GLOBAL.replication_sender_observe_commit_only = FALSE;
include/assert.inc [Reported value matches the one set (set FALSE in GLOBAL scope)]
SET @@GLOBAL.replication_sender_observe_commit_only = 0;
include/assert.inc [Reported value matches the one set (set 0 in GLOBAL scope)]
SET @@GLOBAL.replication_sender_observe_commit_only = ON;
include/assert.inc [Reported value matches the one set (set ON in GLOBAL scope)]
SET @@GLOBAL.replication_sender_observe_commit_only = TRUE;
include/assert.inc [Reported value matches the one set (set TRUE in GLOBAL scope)]
SET @@GLOBAL.replication_sender_observe_commit_only = 1;
include/assert.inc [Reported value matches the one set (set 1 in GLOBAL scope)]
CREATE USER user1;
SET GLOBAL replication_sender_observe_commit_only = 1;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
GRANT SUPER ON *.* TO user1@'%';
Warnings:
Warning	1287	The SUPER privilege identifier is deprecated
FLUSH PRIVILEGES;
SET GLOBAL replication_sender_observe_commit_only = 1;
DROP USER user1;
BEGIN;
SET GLOBAL replication_sender_observe_commit_only = ON;
ROLLBACK;
SET @@global.replication_sender_observe_commit_only = 0;

SET @@global.enforce_gtid_consistency = on;
SET @@global.gtid_mode = off_permissive;
set global sql_replica_skip_counter= 1;
set global sql_replica_skip_counter= 0;
SET @@global.gtid_mode = on_permissive;
set global sql_replica_skip_counter= 1;
set global sql_replica_skip_counter= 0;
SET @@global.gtid_mode = on;
set global sql_replica_skip_counter= 1;
Warnings:
Warning	4016	The value of sql_replica_skip_counter will only take effect for channels running with ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS <> OFF.
include/assert.inc [A warning will be generated stating "The value of sql_replica_skip_counter will only take effect for channels running with"]
select @@global.sql_replica_skip_counter;
@@global.sql_replica_skip_counter
1
set global sql_replica_skip_counter= 0;
select @@global.sql_replica_skip_counter;
@@global.sql_replica_skip_counter
0
SET @@global.gtid_mode = on_permissive;
SET @@global.gtid_mode = off_permissive;
SET @@global.gtid_mode = off;
SET @@global.enforce_gtid_consistency = off;

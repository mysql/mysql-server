CREATE USER 'u1'@'localhost';
#
# TC1. Check unprivileged setting of sub-set of variables
# -------------------------------------------------------
#
SET auto_increment_increment = 1;
SET auto_increment_offset = 1;
SET collation_database = 1;
Warnings:
Warning	1681	Updating 'collation_database' is deprecated. It will be made read-only in a future release.
SET explicit_defaults_for_timestamp = 1;
SET default_collation_for_utf8mb4 = utf8mb4_0900_ai_ci;
Warnings:
Warning	1681	Updating 'default_collation_for_utf8mb4' is deprecated. It will be made read-only in a future release.
SET rbr_exec_mode = IDEMPOTENT;
#
# TC2. Check `REPLICATION_APPLIER` privilege sufficient for setting
#      sub-set of variables
# -----------------------------------------------------------------
#
SET gtid_next = AUTOMATIC;
ERROR 42000: Access denied; you need (at least one of) the SUPER, SYSTEM_VARIABLES_ADMIN, SESSION_VARIABLES_ADMIN or REPLICATION_APPLIER privilege(s) for this operation
SET original_commit_timestamp = 1;
ERROR 42000: Access denied; you need (at least one of) the SUPER, SYSTEM_VARIABLES_ADMIN, SESSION_VARIABLES_ADMIN or REPLICATION_APPLIER privilege(s) for this operation
SET original_server_version = 1;
ERROR 42000: Access denied; you need (at least one of) the SUPER, SYSTEM_VARIABLES_ADMIN, SESSION_VARIABLES_ADMIN or REPLICATION_APPLIER privilege(s) for this operation
SET immediate_server_version = 1;
ERROR 42000: Access denied; you need (at least one of) the SUPER, SYSTEM_VARIABLES_ADMIN, SESSION_VARIABLES_ADMIN or REPLICATION_APPLIER privilege(s) for this operation
SET pseudo_replica_mode = 1;
ERROR 42000: Access denied; you need (at least one of) the SUPER, SYSTEM_VARIABLES_ADMIN, SESSION_VARIABLES_ADMIN or REPLICATION_APPLIER privilege(s) for this operation
GRANT REPLICATION_APPLIER ON *.* TO 'u1'@'localhost';
SET gtid_next = AUTOMATIC;
SET original_commit_timestamp = 1;
SET original_server_version = 1;
SET immediate_server_version = 1;
SET pseudo_replica_mode = 1;
#
# TC3. Check privileges are kept the same while setting sub-set of
#      variables
# ----------------------------------------------------------------
#
SET pseudo_thread_id = 1;
ERROR 42000: Access denied; you need (at least one of) the SUPER, SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege(s) for this operation
SET sql_require_primary_key = 1;
ERROR 42000: Access denied; you need (at least one of) the SUPER, SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege(s) for this operation
SET default_table_encryption = 1;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN and TABLE_ENCRYPTION_ADMIN privilege(s) for this operation
DROP USER 'u1'@'localhost';

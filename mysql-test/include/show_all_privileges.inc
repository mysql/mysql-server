# The purpose of this file is to have more readable and stable
# MTR .result files.
#
# Executing "SHOW GRANTS FOR <user>" will return rows which mentions
# all the static and dynamic privileges assigned to the <user>.
#
# But often times, the output of the above query is big enough and
# clutters the .result file of the test.
#
# This .inc file will simply replace all the static privileges
# with <ALL_STATIC_PRIVILEGES> and all the dynamic privileges
# with <ALL_DYNAMIC_PRIVILEGES> so that instead of having this
#
# ===================================================================
# SHOW GRANTS FOR root@localhost
# GRANT SELECT, INSERT, UPDATE ... DROP ROLE ON *.* TO root@localhost
# GRANT ALLOW_NONEXISTENT_DEFINER,APPLICATION_PASSWORD_ADMIN ... XA_RECOVER_ADMIN ON *.* TO root@localhost
# ===================================================================
#
# We will be having this
#
# ===================================================================
# SHOW GRANTS FOR root@localhost
# GRANT <ALL_STATIC_PRIVILEGES> ON *.* TO root@localhost
# GRANT <ALL_DYNAMIC_PRIVILEGES> ON *.* TO root@localhost
# ===================================================================
#
# To use this file, just use --source include/show_all_privileges.inc
# and just below the .inc statement, execute SHOW GRANTS statement
#
# Example:
# grant ALL PRIVILEGES on *.* to drop_user@localhost with GRANT OPTION;
# grant ALL PRIVILEGES on test.* to drop_user@localhost with GRANT OPTION;
# grant select(a) on test.t1 to drop_user@localhost;
# --source include/show_all_privileges.inc
# show grants for drop_user@localhost;
#
# For every newly added privilege(s), either static or dynamic, please
# update (or add) the hardcoded variables below, and the replace_result
# command (if needed)
#

# Lists out all available static privileges.
--let $static_privileges = SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, RELOAD, SHUTDOWN, PROCESS, FILE, REFERENCES, INDEX, ALTER, SHOW DATABASES, SUPER, CREATE TEMPORARY TABLES, LOCK TABLES, EXECUTE, REPLICATION SLAVE, REPLICATION CLIENT, CREATE VIEW, SHOW VIEW, CREATE ROUTINE, ALTER ROUTINE, CREATE USER, EVENT, TRIGGER, CREATE TABLESPACE, CREATE ROLE, DROP ROLE

# Lists out all dynamic privileges before TP_CONNECTION_ADMIN privilege.
--let $dyn_privs_pre_tp = ALLOW_NONEXISTENT_DEFINER,APPLICATION_PASSWORD_ADMIN,AUDIT_ABORT_EXEMPT,AUDIT_ADMIN,AUTHENTICATION_POLICY_ADMIN,BACKUP_ADMIN,BINLOG_ADMIN,BINLOG_ENCRYPTION_ADMIN,CLONE_ADMIN,CONNECTION_ADMIN,ENCRYPTION_KEY_ADMIN,FIREWALL_EXEMPT,FLUSH_OPTIMIZER_COSTS,FLUSH_PRIVILEGES,FLUSH_STATUS,FLUSH_TABLES,FLUSH_USER_RESOURCES,GROUP_REPLICATION_ADMIN,GROUP_REPLICATION_STREAM,INNODB_REDO_LOG_ARCHIVE,INNODB_REDO_LOG_ENABLE,OPTIMIZE_LOCAL_TABLE,PASSWORDLESS_USER_ADMIN,PERSIST_RO_VARIABLES_ADMIN,REPLICATION_APPLIER,REPLICATION_SLAVE_ADMIN,RESOURCE_GROUP_ADMIN,RESOURCE_GROUP_USER,ROLE_ADMIN,SENSITIVE_VARIABLES_OBSERVER,SERVICE_CONNECTION_ADMIN,SESSION_VARIABLES_ADMIN,SET_ANY_DEFINER,SHOW_ROUTINE,SYSTEM_USER,SYSTEM_VARIABLES_ADMIN,TABLE_ENCRYPTION_ADMIN,TELEMETRY_LOG_ADMIN

--let $dyn_privs_post_tp = TRANSACTION_GTID_TAG,XA_RECOVER_ADMIN

# Lists out all available dynamic privileges
--let $dynamic_privileges = $dyn_privs_pre_tp,$dyn_privs_post_tp

# Lists out all available dynamic privileges along
# with TP_CONNECTION_ADMIN which will be available
# if the thread_pool plugin is installed.
--let $dynamic_privileges_with_tp = $dyn_privs_pre_tp,TP_CONNECTION_ADMIN,$dyn_privs_post_tp

--replace_result $static_privileges <ALL_STATIC_PRIVILEGES> $dynamic_privileges <ALL_DYNAMIC_PRIVILEGES> $dynamic_privileges_with_tp <ALL_DYNAMIC_PRIVILEGES>

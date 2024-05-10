--source include/big_test.inc
--source include/not_valgrind.inc
--source include/mysql_upgrade_preparation.inc

#mysql_upgrade test for user and priviledge tables.

--let $test_error_log= $MYSQL_TMP_DIR/mysql_upgrade_grant.err

--echo #
--echo # Bug #53613: mysql_upgrade incorrectly revokes
--echo #   TRIGGER privilege on given table
--echo #

CREATE USER 'user3'@'%';
GRANT ALL PRIVILEGES ON `roelt`.`test2` TO 'user3'@'%';
--echo Run mysql_upgrade with all privileges on a user

--let $restart_parameters = restart:--upgrade=FORCE --log-error=$test_error_log
--let $wait_counter= 10000
--replace_result $test_error_log test_error_log
--source include/restart_mysqld.inc

SHOW GRANTS FOR 'user3'@'%';

DROP USER 'user3'@'%';

--echo #
--echo # Tests for WL#7194
--echo # Check that users with SUPER privilege (root@localhost and
--echo # the new added user u1) gets XA_RECOVER_ADMIN privilege
--echo # after upgrade.
--echo #

--echo # Show privilege for root@localhost before the privilege XA_RECOVER_ADMIN will be revoked
--let $user = root@localhost
--source include/show_grants.inc

CREATE USER u1;
GRANT SUPER ON *.* TO u1;

--echo # Revoke the privilege XA_RECOVER_ADMIN in order to simulate
--echo # the case when upgrade is run against a database that was created by
--echo # mysql server without support for XA_RECOVER_ADMIN.
REVOKE XA_RECOVER_ADMIN ON *.* FROM root@localhost;
REVOKE XA_RECOVER_ADMIN ON *.* FROM `mysql.session`@localhost;

--echo # We show here that the users root@localhost and u1 have the privilege
--echo # SUPER and don't have the privilege XA_RECOVER_ADMIN
--let $user = root@localhost
--source include/show_grants.inc
SHOW GRANTS FOR u1;

--echo # Start upgrade

--let $restart_parameters = restart:--upgrade=FORCE --log-error=$test_error_log
--let $wait_counter= 10000
--replace_result $test_error_log test_error_log
--source include/restart_mysqld.inc

--echo # Show privileges granted to the users root@localhost and u1
--echo # after upgrade has been finished.
--echo # It is expected that the users root@localhost and u1 have the
--echo # privilege XA_RECOVER_ADMIN granted since they had the privilge SUPER
--echo # before upgrade and there wasn't any user with explicitly granted
--echo # privilege XA_RECOVER_ADMIN.
--let $user = root@localhost
--source include/show_grants.inc
SHOW GRANTS FOR u1;

--echo # Now run upgrade against database where there is a user with granted
--echo # privilege XA_RECOVER_ADMIN and check that for those users who have
--echo # the privilege SUPER assigned the privilege XA_RECOVER_ADMIN won't be
--echo # granted during upgrade.

--echo # Revoke the privilege XA_RECOVER_ADMIN from the user u1 and
--echo # mysql.session@localhost
REVOKE XA_RECOVER_ADMIN ON *.* FROM u1;
REVOKE XA_RECOVER_ADMIN ON *.* FROM `mysql.session`@localhost;

--echo # Start upgrade

--let $restart_parameters = restart:--upgrade=FORCE --log-error=$test_error_log
--let $wait_counter= 10000
--replace_result $test_error_log test_error_log
--source include/restart_mysqld.inc

--echo # It is expected that after upgrade be finished the privilege
--echo # XA_RECOVER_ADMIN won't be granted to the user u1 since
--echo # there was another user (root@localhost) who had the privilege
--echo # XA_RECOVER_ADMIN at the time when upgrade was started
--let $user = root@localhost
--source include/show_grants.inc
SHOW GRANTS FOR u1;

--echo # Cleaning up
DROP USER u1;

--echo # End of tests for WL#7194

--echo #
--echo # Bug#26667007 - MYSQL UPGRADE TO 8.0.3 USER WITH RELOAD GRANTED BACKUP_ADMIN WITH GRANT OPTION
--echo #

--echo # Revoke privileges BACKUP_ADMIN and XA_RECOVER_ADMIN in order to simulate
--echo # the case when upgrade is run against a database that was created by
--echo # mysql server without support for BACKUP_ADMIN/XA_RECOVER_ADMIN.
REVOKE BACKUP_ADMIN ON *.* FROM root@localhost;
REVOKE XA_RECOVER_ADMIN ON *.* FROM root@localhost;
REVOKE BACKUP_ADMIN ON *.* FROM `mysql.session`@localhost;
REVOKE XA_RECOVER_ADMIN ON *.* FROM `mysql.session`@localhost;
CREATE USER u1;
CREATE USER u2;

GRANT RELOAD ON *.* TO u1;
GRANT RELOAD ON *.* TO u2 WITH GRANT OPTION;

GRANT SUPER ON *.* TO u1;
GRANT SUPER ON *.* TO u2 WITH GRANT OPTION;

SHOW GRANTS FOR u1;
SHOW GRANTS FOR u2;

--echo # Start upgrade

--let $restart_parameters = restart:--upgrade=FORCE --log-error=$test_error_log
--let $wait_counter= 10000
--replace_result $test_error_log test_error_log
--source include/restart_mysqld.inc

--echo # Check that the user u1 has privileges BACKUP_ADMIN and XA_RECOVER_ADMIN granted
SHOW GRANTS FOR u1;
--echo # Check that the user u2 has privileges BACKUP_ADMIN and XA_RECOVER_ADMIN granted with grant option
--echo # since originally RELOAD privilege and SUPER privilege were granted to user u2 with grant option
SHOW GRANTS FOR u2;
DROP USER u1;
DROP USER u2;

# Revoke privilege XA_RECOVER_ADMIN from the user mysql.session@localhost in order to
# match contol checksum for mysql.global_grants
REVOKE XA_RECOVER_ADMIN ON *.* FROM `mysql.session`@localhost;

--echo #
--echo # Bug#26948662 - REMOVE SUPER_ACL CHECK IN RESOURCE GROUPS.
--echo #

REVOKE RESOURCE_GROUP_ADMIN ON *.* FROM root@localhost;
REVOKE RESOURCE_GROUP_ADMIN ON *.* FROM `mysql.session`@localhost;


# Create a new user user1 and grant super privilege to user1.
CREATE USER user1;
GRANT SUPER ON *.* TO user1;
--echo # Users root@localhost and user1 have privilege SUPER but not RESOURCE_GROUP_ADMIN 
--let $user = root@localhost
--source include/show_grants.inc
SHOW GRANTS FOR user1;

--echo # Start upgrade

--let $restart_parameters = restart:--upgrade=FORCE --log-error=$test_error_log
--let $wait_counter= 10000
--replace_result $test_error_log test_error_log
--source include/restart_mysqld.inc

--echo # After upgrade users root@localhost and user1 has privilege RESOURCE_GROUP_ADMIN.
--let $user = root@localhost
--source include/show_grants.inc
--let $user = user1
--source include/show_grants.inc
DROP USER user1;

# Revoke privilege RESOURCE_GROUP_ADMIN from the user mysql.session@localhost in order to
# match contol checksum for mysql.global_grants
REVOKE RESOURCE_GROUP_ADMIN ON *.* FROM `mysql.session`@localhost;

--echo #
--echo # Tests for WL#12138
--echo # Check that users with SUPER privilege (root@localhost and
--echo # the new added user u1) get SERVICE_CONNECTION_ADMIN privilege
--echo # after upgrade.
--echo #

--echo # Show privilege for root@localhost before the privilege SERVICE_CONNECTION_ADMIN be revoked
--let $user = root@localhost
--source include/show_grants.inc

CREATE USER u1;
GRANT SUPER ON *.* TO u1;

--echo # Revoke the privilege SERVICE_CONNECTION_ADMIN in order to simulate
--echo # the case when upgrade is run against a database that was created by
--echo # mysql server without support for SERVICE_CONNECTION_ADMIN.
REVOKE SERVICE_CONNECTION_ADMIN ON *.* FROM root@localhost;
REVOKE SERVICE_CONNECTION_ADMIN ON *.* FROM `mysql.session`@localhost;

--echo # We show here that the users root@localhost and u1 have the privilege
--echo # SUPER and don't have the privilege SERVICE_CONNECTION_ADMIN
--let $user = root@localhost
--source include/show_grants.inc
SHOW GRANTS FOR u1;

--let $restart_parameters = restart:--upgrade=FORCE --log-error=$test_error_log
--let $wait_counter= 10000
--replace_result $test_error_log test_error_log
--source include/restart_mysqld.inc

--echo # Show privileges granted to the users root@localhost and u1
--echo # after upgrade has been finished.
--echo # It is expected that the users root@localhost and u1 have the
--echo # privilege SERVICE_CONNECTION_ADMIN granted since they had the privilege SUPER
--echo # before upgrade and there wasn't any user with explicitly granted
--echo # privilege SERVICE_CONNECTION_ADMIN.
--let $user = root@localhost
--source include/show_grants.inc
SHOW GRANTS FOR u1;

--echo # Now run upgrade against database where there is a user with granted
--echo # privilege SERVICE_CONNECTION_ADMIN and check that for those users who have
--echo # the privilege SUPER assigned the privilege SERVICE_CONNECTION_ADMIN won't be
--echo # granted during upgrade.

--echo # Revoke the privilege SERVICE_CONNECTION_ADMIN from the user u1 and
--echo # mysql.session@localhost
REVOKE SERVICE_CONNECTION_ADMIN ON *.* FROM u1;
REVOKE SERVICE_CONNECTION_ADMIN ON *.* FROM `mysql.session`@localhost;

--echo # Start upgrade

--let $restart_parameters = restart:--upgrade=FORCE --log-error=$test_error_log
--let $wait_counter= 10000
--replace_result $test_error_log test_error_log
--source include/restart_mysqld.inc

--echo # It is expected that after upgrade be finished the privilege
--echo # SERVICE_CONNECTION_ADMIN won't be granted to the user u1 since
--echo # there was another user (root@localhost) who had the privilege
--echo # SERVICE_CONNECTION_ADMIN at the time when upgrade was started
--let $user = root@localhost
--source include/show_grants.inc
SHOW GRANTS FOR u1;

--echo # Cleaning up
DROP USER u1;

--echo # End of tests for WL#12138

--echo #
--echo # Bug #29043233: UPGRADE TO 8.0.X, ROOT USER IS NOT REVISED TO INCLUDE ALL DYNAMIC PRIVILEGES
--echo # Bug #29770732: UPGRADE TO 8.0.X, ROOT IS NOT REVISED TO INCLUDE AUDIT_ADMIN DYNAMIC PRIVILEGE
--echo # Bug #30783149: ROOT USER DOES NOT HAVE REPLICATION_APPLIER PRIVLIEGE AFTER UPGRADE TO 8.0.19
--echo #

--let $privileges = AUDIT_ADMIN, BINLOG_ADMIN, BINLOG_ENCRYPTION_ADMIN, CONNECTION_ADMIN, ENCRYPTION_KEY_ADMIN, GROUP_REPLICATION_ADMIN, PERSIST_RO_VARIABLES_ADMIN, REPLICATION_SLAVE_ADMIN, RESOURCE_GROUP_USER, ROLE_ADMIN, SESSION_VARIABLES_ADMIN, SYSTEM_VARIABLES_ADMIN, REPLICATION_APPLIER
--echo set @privileges = $privileges

--echo # Show privileges for root@localhost before the @privileges be revoked
--let $user = root@localhost
--source include/show_grants.inc

CREATE USER u1;
GRANT SUPER ON *.* TO u1;

--echo # Revoke the @privileges in order to simulate
--echo # the case when upgrade is run against a database that was created by
--echo # mysql server without support for them.
--eval REVOKE $privileges ON *.* FROM root@localhost;
--eval REVOKE $privileges ON *.* FROM `mysql.session`@localhost;

--echo # Show here that the users root@localhost and u1 have the privilege
--echo # SUPER and don't have the @privileges
--let $user = root@localhost
--source include/show_grants.inc
SHOW GRANTS FOR u1;

--let $restart_parameters = restart:--upgrade=FORCE --log-error=$test_error_log
--let $wait_counter= 10000
--replace_result $test_error_log test_error_log
--source include/restart_mysqld.inc

--echo # Show privileges granted to the users root@localhost and u1 after
--echo # upgrade has been finished. It is expected that the users
--echo # root@localhost and u1 have the @privileges granted
--echo # since they had the privilege SUPER before upgrade
--echo # and there wasn't any user with explicitly granted @privileges
--let $user = root@localhost
--source include/show_grants.inc
SHOW GRANTS FOR u1;

--echo # Upgrade against database where there is a user with granted @privileges

--echo # Revoke the @privileges from the user u1 and mysql.session@localhost
--eval REVOKE $privileges ON *.* FROM u1;
--eval REVOKE $privileges ON *.* FROM `mysql.session`@localhost;

--echo # Start upgrade

--let $restart_parameters = restart:--upgrade=FORCE --log-error=$test_error_log
--let $wait_counter= 10000
--replace_result $test_error_log test_error_log
--source include/restart_mysqld.inc

--echo # It is expected that after upgrade be finished the @privileges won't be
--echo # granted to the user u1 since there was another user (root@localhost)
--echo # who had the @privileges at the time when upgrade was started
--let $user = root@localhost
--source include/show_grants.inc
SHOW GRANTS FOR u1;

--echo # Cleaning up
DROP USER u1;

--echo ######################################################################
--echo # Test for WL#9049 "Add a dynamic privilege for stored routine backup"
--echo #
--echo #    User having global SELECT privilege, is to be granted
--echo #    SHOW_ROUTINE privilege upon upgrade (provided that there
--echo #    isn't a user who already has the SHOW_ROUTINE privilege).
--echo #
--echo ######################################################################

--echo # Revoke SHOW_ROUTINE from root user (since the upgrade scenario takes place only if no user had this privilege before)
REVOKE SHOW_ROUTINE ON *.* FROM root@localhost;

--echo # Create new user
CREATE USER sheldon;

--echo # Grant global select privilege to new user
GRANT SELECT ON *.* TO sheldon;

--echo # Show grants before upgrade
--let $user = root@localhost
--source include/show_grants.inc
SHOW GRANTS FOR sheldon;

--echo # Start upgrade

--let $restart_parameters = restart:--upgrade=FORCE --log-error=$test_error_log
--let $wait_counter= 10000
--replace_result $test_error_log test_error_log
--source include/restart_mysqld.inc

--echo # Show grants after upgrade
--echo # Should contain SHOW_ROUTINE privilege in both cases
--let $user = root@localhost
--source include/show_grants.inc
SHOW GRANTS FOR sheldon;

--echo # Cleanup
DROP USER sheldon;

--echo #
--echo # WL#15819: New privilege to control OPTIMIZE LOCAL TABLE Operation
--echo #

--echo # User having SYSTEN_USER privilege, is to be granted
--echo # OPTIMIZE_LOCAL_TABLE privilege upon upgrade (provided that there
--echo # isn't a user who already has the OPTIMIZE_LOCAL_TABLE privilege).
--echo #

--echo # Revoke OPTIMIZE_LOCAL_TABLE from root user (since the upgrade scenario
--echo # takes place only if no user had this privilege before)
REVOKE OPTIMIZE_LOCAL_TABLE ON *.* FROM root@localhost;

--echo # Create new user
CREATE USER user1;

--echo # Grant global select privilege to new user
GRANT SYSTEM_USER ON *.* TO user1;

--echo # Show grants before upgrade
SHOW GRANTS FOR root@localhost;
SHOW GRANTS FOR user1;

--echo # Start upgrade

--let $restart_parameters = restart:--upgrade=FORCE --log-error=$test_error_log
--let $wait_counter= 10000
--replace_result $test_error_log test_error_log
--source include/restart_mysqld.inc

--echo # Show grants after upgrade
--echo # Should contain OPTIMIZE_LOCAL_TABLE privilege in both cases
SHOW GRANTS FOR root@localhost;
SHOW GRANTS FOR user1;

--echo # Now run upgrade against database where there is a user with granted
--echo # privilege OPTIMIZE_LOCAL_TABLE and check that for those users who have
--echo # the privilege SYSTEM_USER won't be granted OPTIMIZE_LOCAL_TABLE
--echo # privilege during upgrade.

--echo # Revoke the OPTIMIZE_LOCAL_TABLE privilege from user1
REVOKE OPTIMIZE_LOCAL_TABLE ON *.* FROM user1;

--echo # Start upgrade

--let $restart_parameters = restart:--upgrade=FORCE --log-error=$test_error_log
--let $wait_counter= 10000
--replace_result $test_error_log test_error_log
--source include/restart_mysqld.inc

--echo # It is expected that after upgrade is finished the privilege
--echo # OPTIMIZE_LOCAL_TABLE won't be granted to user1 since
--echo # there was another user (root@localhost) who had the privilege
--echo # OPTIMIZE_LOCAL_TABLE at the time when upgrade was started
SHOW GRANTS FOR root@localhost;
SHOW GRANTS FOR user1;

--echo # Cleanup
DROP USER user1;

--source include/mysql_upgrade_cleanup.inc

--echo End of tests

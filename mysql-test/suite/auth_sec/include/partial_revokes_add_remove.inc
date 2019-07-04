###############################################################################
#                                                                             #
#  This script executes partial_revokes scenarios in following modes :        #
#  1. Normal        :   Script executes on single server                      #
#  2. Replication   :   Script executes in replication setup                  #
#                       on a master and a slave                               #
#                                                                             #
#   Arguments required:                                                       #
#   1. $TEST_MODE - [NORMAL | RPL]                                            #
#                                                                             #
###############################################################################

# If required parameters are not set then throw error and exit.
--let PARAM_PERL_QUIT=0
if ($TEST_MODE == '')
{
--let VARIABLE_NAME = TEST_MODE
--let PARAM_PERL_QUIT=1
}

--perl
    $variable_name = "$ENV{VARIABLE_NAME}";
    die "Error: partial_revokes_add_remove.inc: input variable $variable_name is missing!"
             if ($ENV{PARAM_PERL_QUIT} == 1);
EOF
CREATE DATABASE pr_db;
CREATE DATABASE normal_db;
CREATE TABLE pr_db.t1(c1 int, c2 int);
INSERT INTO pr_db.t1 values (1,2), (3,4);
CREATE TABLE normal_db.t1(c1 int, c2 int);
INSERT INTO normal_db.t1 values (1,2), (3,4);
--let $conn_name = foo_master
--let $reconn_name = foo_master_reconn
--let $bar_conn = bar_master
--echo -------------------------------------------------------------------------
--echo Tests related to users
--echo -------------------------------------------------------------------------
--echo #
--echo # 1. Grantor does not have restriction list but has global priv
--echo #
CREATE USER foo, bar;
GRANT INSERT,SELECT ON *.* TO foo WITH GRANT OPTION;

--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--echo # Must fail since no priv granted to bar yet.
--error ER_NONEXISTING_GRANT
REVOKE INSERT ON mysql.* FROM bar;
--echo # Create global insert priv with partial revoke for bar.
GRANT INSERT ON *.* TO bar;
REVOKE INSERT ON mysql.* FROM bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--echo # Clear partial revoke from bar but global INSERT priv remains as it is
GRANT INSERT ON *.* TO bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--echo # Create the partial revoke on mysql db again.
REVOKE INSERT ON mysql.* FROM bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--echo # Remove partial revoke only
GRANT INSERT ON mysql.* TO bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--echo # Grant DB level privilege only
GRANT INSERT ON mysql.* TO bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
disconnect $conn_name;
DROP USER foo, bar;

--echo #
--echo # 2. Grantor has restriction list and grantee has global priv
--echo #
CREATE USER foo, bar;
GRANT INSERT, SELECT, UPDATE ON *.* TO foo WITH GRANT OPTION;
--echo # Restrict INSERT on mysql DB.
REVOKE INSERT ON mysql.* FROM foo;
SHOW GRANTS FOR foo;
--echo # Restrict UPDATE on mysql DB as well.
REVOKE UPDATE ON mysql.* FROM foo;
SHOW GRANTS FOR foo;
--echo # Must fail, DELETE privilege is not granted
--error ER_NONEXISTING_GRANT
REVOKE DELETE ON mysql.* FROM foo;
GRANT INSERT ON *.* TO bar;
--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--error ER_DBACCESS_DENIED_ERROR
GRANT INSERT ON mysql.* TO bar;
--error ER_DBACCESS_DENIED_ERROR
REVOKE INSERT ON mysql.* FROM bar;
--error ER_DB_ACCESS_DENIED
REVOKE INSERT ON *.* FROM bar;
--echo # Grantee will not get the restriction list from grantor because
--echo # former already has higher priv than grantor
GRANT INSERT ON *.* TO bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
disconnect $conn_name;
DROP USER foo, bar;

--echo #
--echo # 3. Grantor has restriction list and grantee has DB level privileges
--echo #
CREATE USER foo, bar;
GRANT INSERT, UPDATE, SELECT ON *.* TO foo WITH GRANT OPTION;
--echo # Restrict INSERT and UPDATE on mysql DB from foo.
REVOKE INSERT,UPDATE ON mysql.* FROM foo;
SHOW GRANTS FOR foo;
--echo # Grant DB level INSERT priv on mysql DB to bar.
GRANT INSERT ON mysql.* TO bar;
SHOW GRANTS FOR bar;
--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--error ER_DBACCESS_DENIED_ERROR
REVOKE INSERT ON mysql.* FROM bar;
--error ER_DB_ACCESS_DENIED
REVOKE INSERT ON *.* FROM bar;
--error ER_DBACCESS_DENIED_ERROR
GRANT INSERT ON mysql.* TO bar;
--echo # There should not be any restriction list:
--echo #    INSERT on mysql DB because bar already has access to this DB
--echo #    UPDATE on mysql DB because global UPDATE priv is not being granted
GRANT INSERT ON *.* TO bar;
SHOW GRANTS FOR bar;
--connection default
--echo # Should be able to grant/revoke other DB level privileges without
--echo # affecting existing partial revokes.
REVOKE ALL ON *.* FROM bar;
--connection $conn_name
GRANT INSERT ON *.* TO bar;
GRANT INSERT ON `pr_db`.* TO bar;
SHOW GRANTS FOR bar;
--echo # Revoke the privilge. Partial revokes should remain intact
REVOKE INSERT ON `pr_db`.* FROM bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
disconnect $conn_name;
DROP USER foo, bar;

--echo #
--echo # 4. Grantor and grantee both have restriction list
--echo #
--echo # 4.1 Restrictions are aggregated
CREATE USER foo, bar;
GRANT INSERT, SELECT, UPDATE ON *.* TO foo WITH GRANT OPTION;
--echo # Restrict INSERT, UPDATE on mysql DB from foo.
REVOKE INSERT, UPDATE ON mysql.* FROM foo;
SELECT user, user_attributes FROM mysql.user WHERE user LIKE 'foo';
GRANT DELETE, DROP, UPDATE, SELECT ON *.* TO bar;
--echo # Restrict UPDATE and DROP on mysql DB.
REVOKE UPDATE, DROP ON mysql.* FROM bar;
SHOW GRANTS FOR bar;
--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--echo # Restrictions from Grantor and Grantee must be aggregated
GRANT INSERT ON *.* TO bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
--disconnect $conn_name
REVOKE ALL ON *.* FROM foo, bar;
--echo # 4.2 Restrictions are cleared
GRANT SELECT, CREATE, ALTER ON *.* TO foo WITH GRANT OPTION;
REVOKE SELECT ON mysql.* FROM foo;
GRANT SELECT, CREATE ON *.* TO bar;
REVOKE CREATE ON mysql.* FROM bar;
SHOW GRANTS FOR foo;
SHOW GRANTS FOR bar;
--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--echo # Aggregated restrictions turns out to be empty
GRANT SELECT, CREATE,ALTER ON *.* TO bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
SHOW GRANTS FOR bar;
disconnect $conn_name;
DROP USER foo, bar;
--echo #
--echo # 5. Grantor has restriction list and grantee does not have any priv
--echo #
CREATE USER foo, bar;
GRANT INSERT, SELECT ON *.* TO foo WITH GRANT OPTION;
--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--echo # Grantor should be able to add revoke on mysql DB on itself
REVOKE INSERT ON mysql.* FROM foo;
SHOW GRANTS FOR foo;
--connect($reconn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $reconn_name]
--echo # Restrictions must remain intact if grantor reconnects and try to
--echo # grant global privilege to self. In other words, it must be no-op.
GRANT INSERT ON *.* TO foo;
SHOW GRANTS FOR foo;
--echo # Grantee should get the restriction_list of grantor
GRANT INSERT, SELECT ON *.* TO bar;
SHOW GRANTS FOR bar;
--echo # Grantor should be able to add additional restrictions list
REVOKE INSERT,SELECT ON `pr_db`.* FROM bar;
SHOW GRANTS FOR bar;
--echo # Additional restrictions of Grantee should be removed.
GRANT INSERT, SELECT ON *.* TO bar;
SHOW GRANTS FOR bar;
--echo # Should remove, the  global privileges and restrictions both
REVOKE SELECT, INSERT ON *.* FROM bar;
SHOW GRANTS FOR bar;
--echo # Grantee should get the restriction_list of grantor again
GRANT INSERT, SELECT ON *.* TO bar;
SHOW GRANTS FOR bar;
--echo # Remove only global privilege from grantee
REVOKE SELECT ON *.* FROM bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
disconnect $conn_name;
disconnect $reconn_name;
DROP USER foo, bar;

--echo #
--echo # 6. Revoke global privilege as well restriction list from grantee
--echo #    when grantor does not have restriction list
--echo #
CREATE USER foo, bar;
GRANT SELECT, INSERT ON *.* TO foo WITH GRANT OPTION;
--echo # User already have DB level priv
GRANT INSERT ON mysql.* TO bar;
--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--echo # Grant global INSERT privilege to bar
GRANT INSERT ON *.* TO bar;
--echo # Revoke INSERT on mysql DB from bar
REVOKE INSERT ON mysql.* FROM bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--echo # Add partial revoke INSERT on mysql DB for bar.
REVOKE INSERT ON mysql.* FROM bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--echo # Revoke Global privilege as well  as partial revoke.
REVOKE INSERT ON *.* FROM bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
disconnect $conn_name;
--connection default
DROP USER foo,bar;

--echo #
--echo # 7. Granting a global privilege should not affect the restrictions
--echo #    of different privileges on other databases.
--echo #
CREATE USER foo;
CREATE DATABASE  secret;
CREATE DATABASE  meeting;
GRANT INSERT, SELECT, UPDATE ON *.* TO foo;
REVOKE SELECT,INSERT ON secret.* FROM foo;
REVOKE SELECT, UPDATE ON meeting.* FROM foo;
SHOW GRANTS FOR foo;
--echo # Only SELECT restrictions should have removed from
--echo # all databases in the restriction_list
GRANT SELECT ON *.* TO foo;
SHOW GRANTS FOR foo;
DROP USER foo;
DROP DATABASE secret;
DROP DATABASE meeting;

--echo #
--echo # 8. Granting a DB level privilege should not affect the restrictions
--echo #    of different privileges on other databases.
--echo #
CREATE USER foo;
CREATE DATABASE  secret;
CREATE DATABASE  meeting;
GRANT INSERT,DELETE, SELECT, UPDATE ON *.* TO foo;
GRANT SELECT,INSERT ON meeting.* TO foo;
GRANT DELETE,UPDATE ON secret.* TO foo;
REVOKE SELECT,INSERT ON secret.* FROM foo;
REVOKE DELETE, UPDATE ON meeting.* FROM foo;
--echo # Grant sub-set from the set of already granted privileges
GRANT SELECT ON meeting.* TO foo WITH GRANT OPTION;
--echo # The restrictions should not have modified.
SHOW GRANTS FOR foo;
--echo # Grant all privileges from set of already granted privileges
GRANT SELECT,INSERT ON meeting.* TO foo WITH GRANT OPTION;
--echo # The restrictions should not have modified.
SHOW GRANTS FOR foo;
--echo # Grant sub-set from the set of already granted privileges
GRANT UPDATE ON secret.* TO foo;
--echo # The restrictions should not have modified.
SHOW GRANTS FOR foo;
--echo # Grant all privileges from set of already granted privileges
GRANT DELETE,UPDATE ON secret.* TO foo;
--echo # The restrictions should not have modified.
SHOW GRANTS FOR foo;
--echo # Remove the partial revokes
GRANT SELECT,INSERT ON secret.* TO foo;
GRANT DELETE,UPDATE ON meeting.* TO foo;
SHOW GRANTS FOR foo;
DROP USER foo;
DROP DATABASE secret;
DROP DATABASE meeting;

--echo #
--echo # 9. Revoke all privileges from user(s)
--echo #
CREATE USER foo, bar, baz;
CREATE DATABASE secret_db;
--echo #
--echo # 9.1. Grantor has restriction list but grantee does not.
GRANT ALL ON *.* TO foo,baz;
GRANT ALL ON mysql.* TO foo,baz;
GRANT ALL ON mysql.* TO bar;
--echo # All privileges must be cleared from all three users
REVOKE ALL ON *.* FROM foo;
REVOKE ALL ON mysql.* FROM baz;
REVOKE ALL ON mysql.* FROM bar;
SHOW GRANTS FOR foo;
SHOW GRANTS FOR bar;
SHOW GRANTS FOR baz;
DROP USER baz;
--echo # Create partial_revokes for foo
GRANT ALL ON *.* TO foo WITH GRANT OPTION;
REVOKE SELECT ON mysql.* FROM foo;
--echo # GRANT ALL should remove the partial revokes created above
GRANT ALL ON *.* TO foo WITH GRANT OPTION;
SHOW GRANTS FOR foo;
--echo # create partial revokes again
REVOKE SELECT ON mysql.* FROM foo;
SHOW GRANTS FOR foo;
GRANT ALL ON mysql.* TO bar;
--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--error ER_DB_ACCESS_DENIED
REVOKE ALL ON *.* FROM bar;
--connection default
REVOKE ALL ON mysql.* FROM bar;
GRANT ALL ON *.* TO bar;
REVOKE SELECT ON mysql.* FROM bar;
--echo # grantor and grantee both have exact same privileges and partial revokes
SHOW GRANTS FOR foo;
SHOW GRANTS FOR bar;
--connection $conn_name
--echo # grantor should be able to remove the privileges & partial revokes both
REVOKE ALL ON *.* FROM bar;
--connection default
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
--disconnect $conn_name
REVOKE ALL ON *.* FROM foo,bar;
--echo #
--echo # 9.2. REVOKE ALL followed by GLOBAL grants should create restrictions
--connection default
GRANT ALL ON *.* TO foo WITH GRANT OPTION;
--echo # Following should create restrictions for mysql db
REVOKE ALL ON mysql.* FROM foo;
SHOW GRANTS FOR foo;
--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo # restrictions must pass to bar as well
GRANT ALL ON *.* TO bar;
--connection default
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
--disconnect $conn_name
REVOKE ALL ON *.* FROM foo,bar;

--echo #
--echo # 9.3. Grantee has restriction list but grantor does not.
GRANT CREATE USER ON *.* TO foo WITH GRANT OPTION;
GRANT INSERT,SELECT ON *.* TO foo, bar;
REVOKE SELECT,INSERT ON mysql.* FROM bar;
REVOKE INSERT ON secret_db.* FROM bar;
SHOW GRANTS FOR bar;
--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
REVOKE ALL ON *.* FROM bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
--disconnect $conn_name
--echo #
--echo # 9.4. Grantor and grantee both have restriction_list
GRANT INSERT,SELECT ON *.* TO bar;
REVOKE SELECT ON mysql.* FROM bar;
REVOKE SELECT,INSERT ON secret_db.* FROM bar;
SHOW GRANTS FOR bar;
GRANT UPDATE ON *.* TO foo;
REVOKE UPDATE,SELECT ON secret_db.* FROM foo;
SHOW GRANTS FOR foo;
--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--echo # Should fail because foo cannot remove SELECT priv on bar
--error ER_DB_ACCESS_DENIED
REVOKE ALL ON *.* FROM bar;
--connection default
--disconnect $conn_name
GRANT SELECT ON `secret_db`.* TO foo;
SHOW GRANTS FOR foo;
--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--echo # Should work because restriction on foo is different from bar
REVOKE ALL ON *.* FROM bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
--disconnect $conn_name
DROP USER  foo, bar;
DROP DATABASE secret_db;

--echo #
--echo # 10. Revoke a global privilege from grantee which is not present in the
--echo #     grantor's restriction_list
--echo #
CREATE USER foo, bar;
GRANT SELECT,INSERT ON *.* TO foo WITH GRANT OPTION;
REVOKE INSERT ON mysql.* FROM foo;
GRANT SELECT ON *.* TO bar;
SHOW GRANTS FOR foo;
--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--echo # Should work because restriction on foo is different from bar
REVOKE SELECT ON *.* FROM bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
--disconnect $conn_name
DROP USER  foo, bar;

--echo #
--echo # 11. Partial revokes with anonymous user
--echo #
INSERT INTO pr_db.t1 values (1,2), (3,4);
INSERT INTO normal_db.t1 values (1,2), (3,4);
--echo #
--echo # 11.1 Grantor is anonymous user
--echo #
--let $USER_1 = ''@'%'
--let $USER_2 = ''@''
# We can't create ''@'%' unless we delete row from mysql.db
CREATE TEMPORARY TABLE mysql.db_copy AS SELECT * FROM mysql.db;
DELETE FROM mysql.db WHERE host='%';
FLUSH PRIVILEGES;
eval CREATE USER $USER_1, $USER_2;
# Restore mysql.db to its original state
eval GRANT SELECT ON *.* TO $USER_1 WITH GRANT OPTION;
eval REVOKE SELECT ON pr_db.* FROM $USER_1;
--let $conn_name = anonymous_con_1
--connect($conn_name,localhost,anon,,,$MASTER_MYPORT)
--echo [connection $conn_name]
SHOW GRANTS;
SELECT COUNT(*) FROM normal_db.t1;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT COUNT(*) FROM pr_db.t1;
eval GRANT SELECT ON *.* TO $USER_2;
eval SHOW GRANTS FOR $USER_2;
--connection default
--disconnect $conn_name
eval DROP USER $USER_1,$USER_2;
--echo #
--echo # 11.2 Grantee is anonymous user
--echo #
--let $USER_1 = foo@localhost
--let $USER_2 = ''@'%'
eval CREATE USER $USER_1,$USER_2;
eval GRANT SELECT ON *.* TO $USER_1,$USER_2 WITH GRANT OPTION;
--let $conn_name = foo_master
--connect($conn_name,localhost,foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--echo # Create partial revoke for anonymous user from normal user
eval REVOKE SELECT ON pr_db.* FROM $USER_2;
eval SHOW GRANTS FOR $USER_2;
--disconnect $conn_name
--echo # Anynmous user cannot access the restricted database
--let $conn_name = anonymous_con_1
--connect($conn_name,localhost,anon,,,$MASTER_MYPORT)
--echo [connection $conn_name]
SELECT COUNT(*) FROM normal_db.t1;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT COUNT(*) FROM pr_db.t1;
--connection default
--disconnect $conn_name
eval DROP USER $USER_1,$USER_2;
# Restore mysql.db to its original state
DELETE FROM mysql.db;
INSERT INTO mysql.db SELECT * FROM mysql.db_copy;
FLUSH PRIVILEGES;
DROP TABLE mysql.db_copy;
--echo #
--echo # 12. Hybrid scenarios
--echo #
CREATE USER foo, bar;
CREATE DATABASE robo;
GRANT UPDATE, INSERT ON *.* TO bar;
REVOKE UPDATE, INSERT ON robo.* FROM bar;
SHOW GRANTS FOR bar;
--echo # 12.1. Remove the applicable partial revokes(update and insert) as
--echo #       well as grant applicable DB level privileges(select and delete)
GRANT INSERT, UPDATE, DELETE, SELECT ON robo.* TO bar;
SHOW GRANTS FOR bar;
--echo # 12.2. Add the applicable partial revokes(update and insert) as well
--echo #       as revoke applicable DB level privileges(select and delete)
REVOKE INSERT, UPDATE, DELETE, SELECT ON robo.* FROM bar;
SHOW GRANTS FOR bar;
--echo # 12.3 Grant global and DB level privilege then revoke the DB level
       #      privilege as well as create a partial revoke
GRANT SELECT, DROP ON *.* TO foo;
GRANT DROP ON robo.* TO foo;
SHOW GRANTS FOR foo;
REVOKE SELECT, DROP ON robo.* FROM foo;
SHOW GRANTS FOR foo;
# Cleanup
DROP USER foo,bar;
DROP DATABASE robo;
--echo #
--echo # 13. Misc scenarios
--echo #
--echo # 13.1 Revoking DB level privilege multiple times should not
--echo #      create/affect the restrictions
CREATE USER foo;
GRANT ALL ON mysql.* TO foo;
REVOKE SELECT ON mysql.* FROM foo;
REVOKE SELECT ON mysql.* FROM foo;
SHOW GRANTS FOR foo;
--echo # 13.2 Granting DB level privilege multiple times should not
--echo #      create/affect the restrictions
REVOKE ALL ON mysql.* FROM foo;
GRANT SELECT ON mysql.* TO foo;
GRANT SELECT ON mysql.* TO foo;
SHOW GRANTS FOR foo;
DROP USER foo;

--echo ------------Tests related to Users end here -----------------------------

--echo -------------------------------------------------------------------------
--echo Tests related to Roles
--echo -------------------------------------------------------------------------

--echo #
--echo # 1. Activate/Deactivate roles with partial revokes
--echo #
CREATE USER foo, bar;
CREATE ROLE insert_role, select_role;
GRANT INSERT ON *.* TO insert_role WITH GRANT OPTION;
REVOKE INSERT ON pr_db.* FROM insert_role;
GRANT SELECT ON *.* TO select_role WITH GRANT OPTION;
REVOKE SELECT ON pr_db.* FROM select_role;
GRANT ROLE_ADMIN ON *.* TO foo;
GRANT select_role TO foo;
GRANT insert_role TO foo;
SHOW GRANTS FOR foo;
--let $conn_name = foo_master
--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--echo # Must fail because foo does not have the SELECT privilege
--error ER_TABLEACCESS_DENIED_ERROR
SELECT COUNT(*) FROM mysql.user WHERE user = 'foo';
--echo # 1.1. Activate the select_role, now SELECT should work except on pr_db
SET ROLE select_role;
SHOW GRANTS;
SELECT COUNT(*) FROM normal_db.t1;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT COUNT(*) FROM pr_db.t1;
--echo # Grant roles to another user 'bar'
GRANT select_role TO bar;
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--echo # 1.2. Activate the insert_role and deactivate the select_role
SET ROLE ALL EXCEPT select_role;
SHOW GRANTS;
--echo # select must fail but insert should work
--error ER_TABLEACCESS_DENIED_ERROR
SELECT COUNT(*) FROM normal_db.t1;
INSERT INTO normal_db.t1 values (5,6);
--echo # Grant roles to another user 'bar'
GRANT insert_role TO bar;
--connection default
SHOW GRANTS FOR bar;
--connection $conn_name
--source ./partial_revokes_rpl_helper.inc
--echo # 1.3. Deactivate the select_role activated before
SET ROLE NONE;
SHOW GRANTS;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT COUNT(*) FROM normal_db.t1;
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO normal_db.t1 values (5,6);
--echo # 1.4. Activate all roles
SET ROLE ALL;
SHOW GRANTS;
INSERT INTO normal_db.t1 values (7,8);
SELECT COUNT(*) FROM normal_db.t1;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT COUNT(*) FROM pr_db.t1;
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO pr_db.t1 values (7,8);
GRANT insert_role, select_role TO bar;
--connection default
SHOW GRANTS FOR bar;
--connection $conn_name
--source ./partial_revokes_rpl_helper.inc
--echo # Deactivate the select_role activated before
SET ROLE NONE;
--connection default
--echo #
--echo # 2. Default roles with partial revokes
--echo #
CREATE USER baz;
GRANT select_role to baz;
SHOW GRANTS FOR foo;
SHOW GRANTS FOR bar;
SHOW GRANTS FOR baz;
ALTER USER foo DEFAULT ROLE ALL;
SET DEFAULT ROLE select_role TO baz;
--connection $conn_name
--echo [connection $conn_name]
SET ROLE DEFAULT;
SHOW GRANTS;
INSERT INTO normal_db.t1 values (9,10);
SELECT COUNT(*) FROM normal_db.t1;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT COUNT(*) FROM pr_db.t1;
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO pr_db.t1 values (9,10);
GRANT insert_role, select_role TO bar;
--connection default
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--let $conn_name = baz_master
--connect($conn_name, localhost, baz,,,$MASTER_MYPORT)
--echo [connection $conn_name]
SET ROLE DEFAULT;
SHOW GRANTS;
SELECT COUNT(*) FROM normal_db.t1;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT COUNT(*) FROM pr_db.t1;
--connection default
--disconnect $conn_name
DROP USER baz;
--echo # Reset the default roles granted to foo earlier
--let $conn_name = foo_master
--connection $conn_name
--echo [connection $conn_name]
SET ROLE NONE;
--connection default
SET DEFAULT ROLE NONE TO foo;
--echo #
--echo # 3. Mandatory roles with partial revokes
--echo #
CREATE ROLE delete_role;
GRANT DELETE ON *.* TO delete_role;
REVOKE DELETE ON pr_db.* FROM delete_role;
SET GLOBAL mandatory_roles='delete_role';
--connection $conn_name
--echo [connection $conn_name]
SET ROLE delete_role;
SHOW GRANTS;
DELETE FROM normal_db.t1;
--error ER_TABLEACCESS_DENIED_ERROR
DELETE FROM pr_db.t1;
--connection default
SET GLOBAL mandatory_roles='';
DROP ROLE delete_role;
--disconnect $conn_name
DROP USER foo, bar;
DROP ROLE insert_role, select_role;

--echo #
--echo # 4. Aggregation should not reduce privileges
--echo #
--echo [connection default]
CREATE USER bar;
CREATE ROLE r1;
GRANT SELECT ON *.* TO r1, bar;
REVOKE SELECT ON mysql.* FROM r1;
GRANT INSERT ON *.* TO r1;
GRANT r1 TO bar;
SHOW GRANTS FOR bar;
SHOW GRANTS FOR bar USING 'r1';

--connect($bar_conn, localhost, bar,,,$MASTER_MYPORT)
--echo [connection $bar_conn]
SET ROLE ALL;
SELECT user FROM mysql.user WHERE user LIKE 'bar';
SHOW GRANTS;
--connection default
--echo [connection default]
REVOKE INSERT ON *.* FROM r1;
GRANT SELECT ON *.* TO r1, bar;
REVOKE SELECT ON mysql.* FROM bar;
SHOW GRANTS FOR bar;
SHOW GRANTS FOR bar USING 'r1';

--connection $bar_conn
--echo [connection $bar_conn]
SET ROLE ALL;
SELECT user FROM mysql.user WHERE user LIKE 'bar';

--connection default
--echo [connection default]
--disconnect $bar_conn
DROP ROLE r1;
DROP USER bar;

--echo #
--echo # 5. Restriction list of roles should apply to grantee
--echo #
--echo [connection default]
CREATE USER foo, bar;
CREATE ROLE r1;
GRANT SELECT ON *.* TO r1 WITH GRANT OPTION;
REVOKE SELECT ON mysql.* FROM r1;
GRANT r1 TO foo;

--connect($conn_name, localhost, foo,,,$MASTER_MYPORT)
--echo [connection $conn_name]
SET ROLE ALL;
SHOW GRANTS;
GRANT SELECT ON *.* TO bar;

--connection default
--echo [connection default]
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
--disconnect $conn_name
DROP ROLE r1;
DROP USER foo, bar;

--echo #
--echo # 6. Roles have restrictions but user has global privilege
--echo #
CREATE USER bar;
CREATE ROLE r1;
GRANT SELECT ON *.* TO r1, bar;
REVOKE SELECT ON mysql.* FROM r1;
GRANT r1 TO bar;
--let $conn_name = bar_master
--connect($conn_name, localhost, bar,,,$MASTER_MYPORT)
--echo [connection $conn_name]
SET ROLE ALL;
--echo # Restrictions of roles should not apply since user has higher privilege
SHOW GRANTS;
--echo # User's actual privileges ( and restrictions) must be visible
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
--disconnect $conn_name
DROP USER bar;
DROP ROLE r1;

--echo #
--echo # 7. Roles have global privilege but user has restrictions
--echo #
CREATE USER bar;
CREATE ROLE r1;
GRANT SELECT ON *.* TO r1, bar;
REVOKE SELECT ON mysql.* FROM bar;
GRANT r1 TO bar;
--let $conn_name = bar_master
--connect($conn_name, localhost, bar,,,$MASTER_MYPORT)
--echo [connection $conn_name]
SET ROLE ALL;
--echo # User's restrictions should be removed through grantor(role).
SHOW GRANTS;
--echo # User's actual privileges ( and restrictions) must be visible
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
--disconnect $conn_name
DROP USER bar;
DROP ROLE r1;

--echo #
--echo # 8. Roles negating each other's restrictions
--echo #
CREATE ROLE r1, r2;
CREATE USER bar;
GRANT SELECT,INSERT ON *.* TO r1, r2;
REVOKE SELECT ON mysql.* FROM r1;
REVOKE INSERT ON mysql.* FROM r2;
--echo # Grant privileges and roles to user
GRANT DROP ON *.* TO bar;
GRANT UPDATE ON mysql.* TO bar;
GRANT r1, r2 TO bar;
--echo # Roles privileges and restrictions should apply to user
SHOW GRANTS FOR bar USING r1;
SHOW GRANTS FOR bar USING r2;
--echo # Roles global privileges should negate each others restrictions
SHOW GRANTS FOR bar USING r1, r2;
--let $conn_name = bar_master
--connect($conn_name, localhost, bar,,,$MASTER_MYPORT)
--echo [connection $conn_name]
SET ROLE r1, r2;
SHOW GRANTS;
--source ./partial_revokes_rpl_helper.inc
--connection default
--disconnect $conn_name
DROP USER bar;
DROP ROLE r1,r2;

--echo #
--echo # 9. SET ROLE removes the restrictions from the user. Now, Global
--echo #    grant/revoke should not carry restrictions to grantee
--echo #
CREATE USER foo, bar;
CREATE ROLE r1;

GRANT SELECT, INSERT ON *.* TO bar WITH GRANT OPTION;
REVOKE INSERT ON mysql.* FROM bar;
GRANT INSERT ON *.* TO r1;
GRANT r1 TO bar;
--let $conn_name = bar_master
--connect($conn_name, localhost, bar,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--echo # User should not have restriction for now
SET ROLE ALL;
SHOW GRANTS;
--echo # Grantee should not carry the actual restrictions of grantor
GRANT INSERT ON *.* TO foo;
SHOW GRANTS FOR foo;
--echo # Grantor should be able to remove the global privileges from grantee now
REVOKE INSERT ON *.* FROM foo;
SHOW GRANTS FOR foo;
--echo # Grantor should be able to grant global privilege to self
GRANT INSERT ON *.* TO bar;
SHOW GRANTS;
--connection default
--echo [connection default]
--echo # Since, user granted himself global privilege therefore it's
--echo # restrictions has gone permanently now.
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
--disconnect $conn_name
DROP USER foo, bar;
DROP ROLE r1;

--echo #
--echo # 10. SET ROLE removes the restrictions from the user. Now, DB level
--echo #     grant/revoke should work as intended.
--echo #
CREATE USER foo, bar;
CREATE ROLE r1;

GRANT SELECT, INSERT ON *.* TO bar WITH GRANT OPTION;
REVOKE INSERT ON mysql.* FROM bar;
GRANT INSERT ON *.* TO r1;
GRANT r1 TO bar;
--let $conn_name = bar_master
--connect($conn_name, localhost, bar,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--echo # User should not have restriction for now
SET ROLE ALL;
SHOW GRANTS;
--echo # Grantor should be able to grant DB level privilege to grantee
GRANT INSERT ON mysql.* TO foo;
SHOW GRANTS FOR foo;
--echo # Grantor should be able to revoke DB Level priviulege from grantee
REVOKE INSERT ON mysql.* FROM foo;
SHOW GRANTS FOR foo;
--echo # Should be able to grant DB level privilege to self
GRANT INSERT ON mysql.* TO bar;
SHOW GRANTS;
--connection default
--echo [connection default]
--echo # Since, user granted himself DB level privilege therefore it's
--echo # restrictions has gone permanently now.
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
--disconnect $conn_name
DROP USER foo, bar;
DROP ROLE r1;
--echo #
--echo # 11. SET ROLE removes the restrictions from the user. Now, DB level
--echo #     grant/revoke ALL should work as intended.
--echo #
CREATE USER foo, bar;
CREATE ROLE r1;
GRANT SELECT, INSERT ON *.* TO bar WITH GRANT OPTION;
GRANT ALL ON mysql.* TO bar;
REVOKE INSERT ON mysql.* FROM bar;
REVOKE INSERT ON mysql.* FROM bar;
GRANT INSERT ON *.* TO r1;
GRANT r1 TO bar;
--let $conn_name = bar_master
--connect($conn_name, localhost, bar,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--echo # User should not have restriction for now
SET ROLE ALL;
SHOW GRANTS;
--echo # Grantor should be able to grant ALL DB level privilege to grantee
GRANT ALL ON mysql.* TO foo;
SHOW GRANTS FOR foo;
--echo # Grantor should be able to revoke ALL DB Level privilege from grantee
REVOKE ALL ON mysql.* FROM foo;
SHOW GRANTS FOR foo;
--echo # Should be able to grant ALL DB level privileges to self
GRANT ALL ON mysql.* TO bar;
SHOW GRANTS;
--connection default
--echo [connection default]
--echo # Since, user granted himself all DB level privileges therefore it's
--echo # restrictions has gone permanently now.
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
--disconnect $conn_name
DROP USER foo, bar;
DROP ROLE r1;
--echo #
--echo # 12. SET ROLE removes the restrictions from the user. Now, global
--echo #     grant/revoke ALL should work as intended.
--echo #
CREATE USER foo, bar;
CREATE ROLE r1;
GRANT ALL ON *.* TO bar WITH GRANT OPTION;
REVOKE INSERT ON mysql.* FROM bar;
GRANT INSERT ON *.* TO r1;
GRANT r1 TO bar;
--let $conn_name = bar_master
--connect($conn_name, localhost, bar,,,$MASTER_MYPORT)
--echo [connection $conn_name]
--echo # User should not have restriction for now
SET ROLE ALL;
SHOW GRANTS;
--echo # Grantor should be able to grant ALL global level privilege to grantee
GRANT ALL ON *.* TO foo;
SHOW GRANTS FOR foo;
--echo # Grantor should be able to revoke ALL global level privilege from
--echo # grantee
REVOKE ALL ON *.* FROM foo;
SHOW GRANTS FOR foo;
--echo # Should be able to grant ALL DB level privileges to self
GRANT ALL ON *.* TO bar;
SHOW GRANTS;
--connection default
--echo [connection default]
--echo # Since, user granted himself all DB level privileges therefore it's
--echo # restrictions has gone permanently now.
SHOW GRANTS FOR bar;
--source ./partial_revokes_rpl_helper.inc
--connection default
--disconnect $conn_name
DROP USER foo, bar;
DROP ROLE r1;
--echo ---------------------Tests related to roles end here---------------------

--echo -------------------------------------------------------------------------
--echo Invalid restrictions and privilege combinations
--echo -------------------------------------------------------------------------
CREATE USER foo, bar;
--echo #
--echo # 1. Add INSERT partial revoke on mysql DB for user bar
--echo #
GRANT SELECT, INSERT ON *.* TO bar;
REVOKE SELECT, INSERT ON mysql.* FROM bar;
SHOW GRANTS FOR bar;
INSERT INTO mysql.db(user,host, db, Insert_priv) VALUES('bar', '%', 'mysql', 'Y');
FLUSH PRIVILEGES;
SHOW GRANTS FOR bar;
--error ER_PARTIAL_REVOKE_AND_DB_GRANT_BOTH_EXISTS
GRANT INSERT ON *.* TO bar;
--error ER_PARTIAL_REVOKE_AND_DB_GRANT_BOTH_EXISTS
REVOKE INSERT ON *.* FROM bar;
--error ER_PARTIAL_REVOKE_AND_DB_GRANT_BOTH_EXISTS
GRANT INSERT ON mysql.* TO bar;
--error ER_PARTIAL_REVOKE_AND_DB_GRANT_BOTH_EXISTS
REVOKE INSERT ON mysql.* FROM bar;
--echo # We should not even able to grant/revoke fresh privileges.
--error ER_PARTIAL_REVOKE_AND_DB_GRANT_BOTH_EXISTS
GRANT UPDATE ON mysql.* TO bar;
--error ER_PARTIAL_REVOKE_AND_DB_GRANT_BOTH_EXISTS
REVOKE UPDATE ON mysql.* FROM bar;
--error ER_PARTIAL_REVOKE_AND_DB_GRANT_BOTH_EXISTS
GRANT UPDATE ON *.* TO bar;
--error ER_PARTIAL_REVOKE_AND_DB_GRANT_BOTH_EXISTS
REVOKE UPDATE ON *.* FROM bar;
--connect(conn_bar, localhost, bar,,,$MASTER_MYPORT)
# Should work
SELECT CURRENT_USER();
SELECT COUNT(*) FROM pr_db.t1;
--connection default
disconnect conn_bar;
DROP USER foo,bar;

--echo #
--echo # 2. Add privilege other than DB_ACLs in user attributes column.
--echo #
CREATE USER u1;
CREATE DATABASE db1;
GRANT SELECT, INSERT ON *.* TO u1;
REVOKE SELECT, INSERT ON db1.* FROM u1;
UPDATE mysql.user SET user_attributes='{"Restrictions": [{"Database": "db1", "Privileges": ["SELECT", "INSERT", "SUPER", "DELETE"]}]}' WHERE user = 'u1';
# We let our server restart attempts write to the file $server_log.
let server_log= $MYSQLTEST_VARDIR/log/mysqld.1.err;
let SEARCH_FILE= $server_log;

--echo # Search for : ignored restrictions for privilege(s) 'SUPER' for database 'db1' as these are not valid database privileges
let SEARCH_PATTERN= ignored restrictions for privilege(s) 'SUPER' for database 'db1' as these are not valid database privileges;
--source include/search_pattern.inc
--echo # Search completed.

--echo # Search for : ignored restrictions for privilege(s) 'DELETE' for database 'db1' as corresponding global privilege(s) are not granted
let SEARCH_PATTERN= ignored restrictions for privilege(s) 'DELETE' for database 'db1' as corresponding global privilege(s) are not granted;
--source include/search_pattern.inc
--echo # Search completed.

DROP DATABASE db1;
DROP USER u1;
--echo -Tests related to invalid restrictions & privilege combinations end here-
--echo
--echo -------------------------------------------------------------------------
--echo Partial revokes do not treat wildcard grants specially.
--echo -------------------------------------------------------------------------
CREATE USER foo;
CREATE DATABASE `db_1`;
CREATE DATABASE `db%1`;
GRANT SELECT ON *.* TO foo;

--echo # Create restriction - We dont use '\' to escape '_' or '%'
REVOKE SELECT ON `db_1`.* FROM foo;
REVOKE SELECT ON `db%1`.* FROM foo;

--echo # Should show 2 restrictions
SHOW GRANTS FOR foo;

--echo # Should not remove restrictions on db_1 or db%1,
--echo # instead both grants should be added on db(s) as it is.
GRANT SELECT ON `db\_1`.* TO foo;
GRANT SELECT ON `db\%1`.* TO foo;
SHOW GRANTS FOR foo;

--echo # Should remove two grants added above
REVOKE SELECT ON `db\_1`.* FROM foo;
REVOKE SELECT ON `db\%1`.* FROM foo;

--echo # Should show 2 restrictions
SHOW GRANTS FOR foo;

# Cleanup
REVOKE SELECT ON *.* FROM foo;
DROP USER foo;
DROP DATABASE `db_1`;
DROP DATABASE `db%1`;
--echo -------------------------------------------------------------------------
--echo Test related to wildcard grants end here.
--echo -------------------------------------------------------------------------

#
# Clean up
#
DROP DATABASE pr_db;
DROP DATABASE normal_db;
call mtr.add_suppression(".*Nothing to revoke. Global privilege.*");

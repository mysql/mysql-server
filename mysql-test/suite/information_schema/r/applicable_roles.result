# SETUP SCHEMA
#
# SETUP SCHEMA
#
use test;
#
#  CREATE ROLES
#
CREATE ROLE reader, updater;
GRANT reader TO updater WITH ADMIN OPTION;
CREATE ROLE task1, task1_lead, task2, task2_lead;
GRANT updater TO task1_lead;
GRANT updater TO task2_lead;
GRANT task1 TO task1_lead;
GRANT task2 TO task2_lead;
CREATE ROLE project_lead;
GRANT task1_lead TO project_lead;
GRANT task2_lead TO project_lead;
CREATE ROLE product_lead;
GRANT project_lead TO product_lead;
CREATE ROLE hr_rules, hr_access;
GRANT hr_rules TO hr_access WITH ADMIN OPTION;
#
# USER ACCOUNT AND ASSIGN ROLE's TO THEM.
#
CREATE USER user0 IDENTIFIED BY 'foo';
CREATE USER user1 IDENTIFIED BY 'foo';
CREATE USER user2 IDENTIFIED BY 'foo';
GRANT reader, hr_rules TO user1;
GRANT updater, hr_rules TO user2;
CREATE USER lead_user1 IDENTIFIED BY 'foo';
CREATE USER lead_user2 IDENTIFIED BY 'foo';
GRANT task1_lead, hr_access TO lead_user1;
GRANT task2_lead, hr_access TO lead_user2;
CREATE USER project_lead_user IDENTIFIED BY 'foo';
GRANT project_lead, hr_access TO project_lead_user;
CREATE USER product_lead_user IDENTIFIED BY 'foo';
GRANT product_lead, hr_access TO product_lead_user;
#
# CREATE DATABASE OBJECTS
#
CREATE TABLE task1_ledger1 (f1 INT, f2 INT);
CREATE TABLE task1_ledger2 (f1 INT, f2 INT);
CREATE TABLE task2_ledger1 (f1 INT, f2 INT);
CREATE TABLE task2_ledger2 (f1 INT, f2 INT);
CREATE PROCEDURE daily_report (OUT param1 INT)
BEGIN
SELECT COUNT(*) INTO param1 FROM t1;
END//
CREATE PROCEDURE weekly_report (OUT param1 INT)
BEGIN
SELECT COUNT(*) INTO param1 FROM t1;
END//
CREATE PROCEDURE proc1 (OUT param1 INT)
BEGIN
SELECT COUNT(*) INTO param1 FROM t1;
END//
CREATE TABLE hr_ledger1 (f1 INT, f2 INT);
CREATE TABLE hr_ledger2 (f1 INT, f2 INT);
CREATE TABLE t1 (f1 INT);
#
# GRANTS TO ROLE
#
GRANT SELECT ON task1_ledger1 TO reader;
GRANT UPDATE ON task1_ledger1 TO updater;
GRANT UPDATE ON task1_ledger2 TO updater;
GRANT UPDATE ON task2_ledger1 TO updater;
GRANT UPDATE ON task2_ledger2 TO updater;
GRANT INSERT ON task1_ledger1 TO task1_lead;
GRANT INSERT ON task1_ledger2 TO task1_lead;
GRANT INSERT ON task2_ledger1 TO task2_lead;
GRANT INSERT ON task2_ledger2 TO task2_lead;
GRANT DELETE ON task2_ledger1 TO project_lead;
GRANT DELETE ON task2_ledger2 TO project_lead;
GRANT SELECT ON hr_ledger1 TO hr_rules;
GRANT SELECT ON hr_ledger2 to hr_access;
GRANT SELECT (f2) ON task1_ledger1 to reader;
GRANT SELECT (f2) ON task1_ledger2 to reader WITH GRANT OPTION;
GRANT SELECT (f2) ON task2_ledger2 to product_lead;
GRANT SELECT (f2) ON task2_ledger2 to product_lead WITH GRANT OPTION;
GRANT SELECT (f2) ON hr_ledger1 TO hr_rules;
GRANT SELECT (f2) ON hr_ledger2 to hr_access WITH GRANT OPTION;
GRANT EXECUTE ON PROCEDURE daily_report TO task1_lead;
GRANT EXECUTE ON PROCEDURE weekly_report TO task2_lead;
GRANT ALTER ROUTINE ON PROCEDURE daily_report TO product_lead;
GRANT ALTER ROUTINE ON PROCEDURE weekly_report TO product_lead;
GRANT EXECUTE ON PROCEDURE weekly_report TO hr_rules;
GRANT EXECUTE ON PROCEDURE weekly_report TO hr_access;
GRANT SELECT ON t1 TO user1, user2, lead_user1, lead_user2, project_lead_user, product_lead_user;
GRANT SELECT (f1) ON t1 TO user1, user2, lead_user1, lead_user2, project_lead_user, product_lead_user;
GRANT EXECUTE ON PROCEDURE proc1 TO user1, user2, lead_user1, lead_user2, project_lead_user, product_lead_user;
#
# USER WITH NO ROLES ASSIGNED.
#
# List all APPLICABLE ROLES for user0
SELECT
USER,           /* SQL Standard   */
HOST,           /* MySQL Specific */
GRANTEE,        /* SQL Standard   */
GRANTEE_HOST,   /* MySQL Specific */
ROLE_NAME,      /* SQL Standard   */
ROLE_HOST,      /* MySQL Specific */
IS_GRANTABLE,   /* SQL Standard   */
IS_DEFAULT,     /* MySQL Specific */
IS_MANDATORY    /* MySQL Specific */
FROM INFORMATION_SCHEMA.APPLICABLE_ROLES;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
# USER WITH ROLES.
#
# Case 1: Login as user1
#
# Connect as user1
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
user1	%	user1	%	hr_rules	%	NO	NO	NO
user1	%	user1	%	reader	%	NO	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
NONE
SHOW GRANTS;
Grants for user1@%
GRANT USAGE ON *.* TO `user1`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `user1`@`%`
GRANT `hr_rules`@`%`,`reader`@`%` TO `user1`@`%`
# Enable roles;
SET ROLE hr_rules;
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
user1	%	user1	%	hr_rules	%	NO	NO	NO
user1	%	user1	%	reader	%	NO	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_rules`@`%`
SHOW GRANTS;
Grants for user1@%
GRANT USAGE ON *.* TO `user1`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `user1`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `user1`@`%`
GRANT `hr_rules`@`%`,`reader`@`%` TO `user1`@`%`
Disconnect user1
# Connect as user1
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
SELECT CURRENT_ROLE();
CURRENT_ROLE()
NONE
SHOW GRANTS;
Grants for user1@%
GRANT USAGE ON *.* TO `user1`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `user1`@`%`
GRANT `hr_rules`@`%`,`reader`@`%` TO `user1`@`%`
# Enable roles;
SET ROLE hr_rules;
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_rules`@`%`
SHOW GRANTS;
Grants for user1@%
GRANT USAGE ON *.* TO `user1`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `user1`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `user1`@`%`
GRANT `hr_rules`@`%`,`reader`@`%` TO `user1`@`%`
Disconnect user1
#
# Case 2: Login as user2
#
# Connect as user2
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
user2	%	updater	%	reader	%	YES	NO	NO
user2	%	user2	%	hr_rules	%	NO	NO	NO
user2	%	user2	%	updater	%	NO	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
NONE
SHOW GRANTS;
Grants for user2@%
GRANT USAGE ON *.* TO `user2`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `user2`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `user2`@`%`
GRANT `hr_rules`@`%`,`updater`@`%` TO `user2`@`%`
# Enable roles;
SET ROLE hr_rules;
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
user2	%	updater	%	reader	%	YES	NO	NO
user2	%	user2	%	hr_rules	%	NO	NO	NO
user2	%	user2	%	updater	%	NO	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_rules`@`%`
SHOW GRANTS;
Grants for user2@%
GRANT USAGE ON *.* TO `user2`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `user2`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `user2`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `user2`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `user2`@`%`
GRANT `hr_rules`@`%`,`updater`@`%` TO `user2`@`%`
Disconnect user2
# Connect as user2
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
user2	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
NONE
SHOW GRANTS;
Grants for user2@%
GRANT USAGE ON *.* TO `user2`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `user2`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `user2`@`%`
GRANT `hr_rules`@`%`,`updater`@`%` TO `user2`@`%`
# Enable roles;
SET ROLE hr_rules;
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
user2	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_rules`@`%`
SHOW GRANTS;
Grants for user2@%
GRANT USAGE ON *.* TO `user2`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `user2`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `user2`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `user2`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `user2`@`%`
GRANT `hr_rules`@`%`,`updater`@`%` TO `user2`@`%`
Disconnect user2
#
# Case 3: Login as lead_user1 with mandatory roles.
#
SET GLOBAL mandatory_roles = 'task2';
# Connect as lead_user1
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user1	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user1	%	lead_user1	%	hr_access	%	NO	NO	NO
lead_user1	%	lead_user1	%	task1_lead	%	NO	NO	NO
lead_user1	%	lead_user1	%	task2	%	NO	NO	YES
lead_user1	%	task1_lead	%	task1	%	NO	NO	NO
lead_user1	%	task1_lead	%	updater	%	NO	NO	NO
lead_user1	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
NONE
SHOW GRANTS;
Grants for lead_user1@%
GRANT USAGE ON *.* TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user1`@`%`
GRANT `hr_access`@`%`,`task1_lead`@`%`,`task2`@`%` TO `lead_user1`@`%`
# Enable roles;
SET ROLE NONE;
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user1	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user1	%	lead_user1	%	hr_access	%	NO	NO	NO
lead_user1	%	lead_user1	%	task1_lead	%	NO	NO	NO
lead_user1	%	lead_user1	%	task2	%	NO	NO	YES
lead_user1	%	task1_lead	%	task1	%	NO	NO	NO
lead_user1	%	task1_lead	%	updater	%	NO	NO	NO
lead_user1	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
NONE
SHOW GRANTS;
Grants for lead_user1@%
GRANT USAGE ON *.* TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user1`@`%`
GRANT `hr_access`@`%`,`task1_lead`@`%`,`task2`@`%` TO `lead_user1`@`%`
Disconnect lead_user1
# Connect as lead_user1
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user1	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user1	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
NONE
SHOW GRANTS;
Grants for lead_user1@%
GRANT USAGE ON *.* TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user1`@`%`
GRANT `hr_access`@`%`,`task1_lead`@`%`,`task2`@`%` TO `lead_user1`@`%`
# Enable roles;
SET ROLE NONE;
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user1	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user1	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
NONE
SHOW GRANTS;
Grants for lead_user1@%
GRANT USAGE ON *.* TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user1`@`%`
GRANT `hr_access`@`%`,`task1_lead`@`%`,`task2`@`%` TO `lead_user1`@`%`
Disconnect lead_user1
SET GLOBAL mandatory_roles = 'hr_access';
# Connect as lead_user1
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user1	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user1	%	lead_user1	%	hr_access	%	NO	NO	YES
lead_user1	%	lead_user1	%	task1_lead	%	NO	NO	NO
lead_user1	%	task1_lead	%	task1	%	NO	NO	NO
lead_user1	%	task1_lead	%	updater	%	NO	NO	NO
lead_user1	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
NONE
SHOW GRANTS;
Grants for lead_user1@%
GRANT USAGE ON *.* TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user1`@`%`
GRANT `hr_access`@`%`,`task1_lead`@`%` TO `lead_user1`@`%`
# Enable roles;
SET ROLE hr_access;
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user1	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user1	%	lead_user1	%	hr_access	%	NO	NO	YES
lead_user1	%	lead_user1	%	task1_lead	%	NO	NO	NO
lead_user1	%	task1_lead	%	task1	%	NO	NO	NO
lead_user1	%	task1_lead	%	updater	%	NO	NO	NO
lead_user1	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_access`@`%`
SHOW GRANTS;
Grants for lead_user1@%
GRANT USAGE ON *.* TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger2` TO `lead_user1`@`%` WITH GRANT OPTION
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `lead_user1`@`%`
GRANT `hr_access`@`%`,`task1_lead`@`%` TO `lead_user1`@`%`
GRANT `hr_rules`@`%` TO `lead_user1`@`%` WITH ADMIN OPTION
Disconnect lead_user1
# Connect as lead_user1
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user1	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user1	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
NONE
SHOW GRANTS;
Grants for lead_user1@%
GRANT USAGE ON *.* TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user1`@`%`
GRANT `hr_access`@`%`,`task1_lead`@`%` TO `lead_user1`@`%`
# Enable roles;
SET ROLE hr_access;
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user1	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user1	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_access`@`%`
SHOW GRANTS;
Grants for lead_user1@%
GRANT USAGE ON *.* TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger2` TO `lead_user1`@`%` WITH GRANT OPTION
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `lead_user1`@`%`
GRANT `hr_access`@`%`,`task1_lead`@`%` TO `lead_user1`@`%`
GRANT `hr_rules`@`%` TO `lead_user1`@`%` WITH ADMIN OPTION
Disconnect lead_user1
SET GLOBAL activate_all_roles_on_login=on;
# Connect as lead_user1
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user1	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user1	%	lead_user1	%	hr_access	%	NO	NO	YES
lead_user1	%	lead_user1	%	task1_lead	%	NO	NO	NO
lead_user1	%	task1_lead	%	task1	%	NO	NO	NO
lead_user1	%	task1_lead	%	updater	%	NO	NO	NO
lead_user1	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_access`@`%`,`task1_lead`@`%`
SHOW GRANTS;
Grants for lead_user1@%
GRANT USAGE ON *.* TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger2` TO `lead_user1`@`%` WITH GRANT OPTION
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f2`), INSERT, UPDATE ON `test`.`task1_ledger1` TO `lead_user1`@`%`
GRANT SELECT (`f2`), INSERT, UPDATE ON `test`.`task1_ledger2` TO `lead_user1`@`%` WITH GRANT OPTION
GRANT UPDATE ON `test`.`task2_ledger1` TO `lead_user1`@`%`
GRANT UPDATE ON `test`.`task2_ledger2` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`daily_report` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `lead_user1`@`%`
GRANT `hr_access`@`%`,`task1_lead`@`%` TO `lead_user1`@`%`
GRANT `hr_rules`@`%`,`reader`@`%` TO `lead_user1`@`%` WITH ADMIN OPTION
# Enable roles;
SET ROLE hr_access;
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user1	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user1	%	lead_user1	%	hr_access	%	NO	NO	YES
lead_user1	%	lead_user1	%	task1_lead	%	NO	NO	NO
lead_user1	%	task1_lead	%	task1	%	NO	NO	NO
lead_user1	%	task1_lead	%	updater	%	NO	NO	NO
lead_user1	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_access`@`%`
SHOW GRANTS;
Grants for lead_user1@%
GRANT USAGE ON *.* TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger2` TO `lead_user1`@`%` WITH GRANT OPTION
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `lead_user1`@`%`
GRANT `hr_access`@`%`,`task1_lead`@`%` TO `lead_user1`@`%`
GRANT `hr_rules`@`%` TO `lead_user1`@`%` WITH ADMIN OPTION
Disconnect lead_user1
# Connect as lead_user1
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user1	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user1	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_access`@`%`,`task1_lead`@`%`
SHOW GRANTS;
Grants for lead_user1@%
GRANT USAGE ON *.* TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger2` TO `lead_user1`@`%` WITH GRANT OPTION
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f2`), INSERT, UPDATE ON `test`.`task1_ledger1` TO `lead_user1`@`%`
GRANT SELECT (`f2`), INSERT, UPDATE ON `test`.`task1_ledger2` TO `lead_user1`@`%` WITH GRANT OPTION
GRANT UPDATE ON `test`.`task2_ledger1` TO `lead_user1`@`%`
GRANT UPDATE ON `test`.`task2_ledger2` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`daily_report` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `lead_user1`@`%`
GRANT `hr_access`@`%`,`task1_lead`@`%` TO `lead_user1`@`%`
GRANT `hr_rules`@`%`,`reader`@`%` TO `lead_user1`@`%` WITH ADMIN OPTION
# Enable roles;
SET ROLE hr_access;
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user1	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user1	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_access`@`%`
SHOW GRANTS;
Grants for lead_user1@%
GRANT USAGE ON *.* TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `lead_user1`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger2` TO `lead_user1`@`%` WITH GRANT OPTION
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user1`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `lead_user1`@`%`
GRANT `hr_access`@`%`,`task1_lead`@`%` TO `lead_user1`@`%`
GRANT `hr_rules`@`%` TO `lead_user1`@`%` WITH ADMIN OPTION
Disconnect lead_user1
SET GLOBAL activate_all_roles_on_login=off;
SET GLOBAL mandatory_roles = default;
#
# Case 4: Login as lead_user2 with default roles.
#
SET DEFAULT ROLE hr_access TO lead_user2;
# Connect as lead_user2
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user2	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user2	%	lead_user2	%	hr_access	%	NO	YES	NO
lead_user2	%	lead_user2	%	task2_lead	%	NO	NO	NO
lead_user2	%	task2_lead	%	task2	%	NO	NO	NO
lead_user2	%	task2_lead	%	updater	%	NO	NO	NO
lead_user2	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_access`@`%`
SHOW GRANTS;
Grants for lead_user2@%
GRANT USAGE ON *.* TO `lead_user2`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `lead_user2`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger2` TO `lead_user2`@`%` WITH GRANT OPTION
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user2`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user2`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `lead_user2`@`%`
GRANT `hr_access`@`%`,`task2_lead`@`%` TO `lead_user2`@`%`
GRANT `hr_rules`@`%` TO `lead_user2`@`%` WITH ADMIN OPTION
# Enable roles;
SET ROLE task2_lead;
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user2	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user2	%	lead_user2	%	hr_access	%	NO	YES	NO
lead_user2	%	lead_user2	%	task2_lead	%	NO	NO	NO
lead_user2	%	task2_lead	%	task2	%	NO	NO	NO
lead_user2	%	task2_lead	%	updater	%	NO	NO	NO
lead_user2	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`task2_lead`@`%`
SHOW GRANTS;
Grants for lead_user2@%
GRANT USAGE ON *.* TO `lead_user2`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user2`@`%`
GRANT SELECT, SELECT (`f2`), UPDATE ON `test`.`task1_ledger1` TO `lead_user2`@`%`
GRANT SELECT (`f2`), UPDATE ON `test`.`task1_ledger2` TO `lead_user2`@`%` WITH GRANT OPTION
GRANT INSERT, UPDATE ON `test`.`task2_ledger1` TO `lead_user2`@`%`
GRANT INSERT, UPDATE ON `test`.`task2_ledger2` TO `lead_user2`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user2`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `lead_user2`@`%`
GRANT `hr_access`@`%`,`task2_lead`@`%` TO `lead_user2`@`%`
GRANT `reader`@`%` TO `lead_user2`@`%` WITH ADMIN OPTION
Disconnect lead_user2
# Connect as lead_user2
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user2	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user2	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_access`@`%`
SHOW GRANTS;
Grants for lead_user2@%
GRANT USAGE ON *.* TO `lead_user2`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `lead_user2`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger2` TO `lead_user2`@`%` WITH GRANT OPTION
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user2`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user2`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `lead_user2`@`%`
GRANT `hr_access`@`%`,`task2_lead`@`%` TO `lead_user2`@`%`
GRANT `hr_rules`@`%` TO `lead_user2`@`%` WITH ADMIN OPTION
# Enable roles;
SET ROLE task2_lead;
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
lead_user2	%	hr_access	%	hr_rules	%	YES	NO	NO
lead_user2	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`task2_lead`@`%`
SHOW GRANTS;
Grants for lead_user2@%
GRANT USAGE ON *.* TO `lead_user2`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `lead_user2`@`%`
GRANT SELECT, SELECT (`f2`), UPDATE ON `test`.`task1_ledger1` TO `lead_user2`@`%`
GRANT SELECT (`f2`), UPDATE ON `test`.`task1_ledger2` TO `lead_user2`@`%` WITH GRANT OPTION
GRANT INSERT, UPDATE ON `test`.`task2_ledger1` TO `lead_user2`@`%`
GRANT INSERT, UPDATE ON `test`.`task2_ledger2` TO `lead_user2`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `lead_user2`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `lead_user2`@`%`
GRANT `hr_access`@`%`,`task2_lead`@`%` TO `lead_user2`@`%`
GRANT `reader`@`%` TO `lead_user2`@`%` WITH ADMIN OPTION
Disconnect lead_user2
SET DEFAULT ROLE NONE TO lead_user1;
#
# Case 5: Login as project_lead_user with role graph.
#
# Connect as project_lead_user
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
project_lead_user	%	hr_access	%	hr_rules	%	YES	NO	NO
project_lead_user	%	project_lead	%	task1_lead	%	NO	NO	NO
project_lead_user	%	project_lead	%	task2_lead	%	NO	NO	NO
project_lead_user	%	project_lead_user	%	hr_access	%	NO	NO	NO
project_lead_user	%	project_lead_user	%	project_lead	%	NO	NO	NO
project_lead_user	%	task1_lead	%	task1	%	NO	NO	NO
project_lead_user	%	task1_lead	%	updater	%	NO	NO	NO
project_lead_user	%	task2_lead	%	task2	%	NO	NO	NO
project_lead_user	%	task2_lead	%	updater	%	NO	NO	NO
project_lead_user	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
NONE
SHOW GRANTS;
Grants for project_lead_user@%
GRANT USAGE ON *.* TO `project_lead_user`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `project_lead_user`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `project_lead_user`@`%`
GRANT `hr_access`@`%`,`project_lead`@`%` TO `project_lead_user`@`%`
# Enable roles;
SET ROLE hr_access;
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
project_lead_user	%	hr_access	%	hr_rules	%	YES	NO	NO
project_lead_user	%	project_lead	%	task1_lead	%	NO	NO	NO
project_lead_user	%	project_lead	%	task2_lead	%	NO	NO	NO
project_lead_user	%	project_lead_user	%	hr_access	%	NO	NO	NO
project_lead_user	%	project_lead_user	%	project_lead	%	NO	NO	NO
project_lead_user	%	task1_lead	%	task1	%	NO	NO	NO
project_lead_user	%	task1_lead	%	updater	%	NO	NO	NO
project_lead_user	%	task2_lead	%	task2	%	NO	NO	NO
project_lead_user	%	task2_lead	%	updater	%	NO	NO	NO
project_lead_user	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_access`@`%`
SHOW GRANTS;
Grants for project_lead_user@%
GRANT USAGE ON *.* TO `project_lead_user`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `project_lead_user`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger2` TO `project_lead_user`@`%` WITH GRANT OPTION
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `project_lead_user`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `project_lead_user`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `project_lead_user`@`%`
GRANT `hr_access`@`%`,`project_lead`@`%` TO `project_lead_user`@`%`
GRANT `hr_rules`@`%` TO `project_lead_user`@`%` WITH ADMIN OPTION
Disconnect project_lead_user
# Connect as project_lead_user
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
project_lead_user	%	hr_access	%	hr_rules	%	YES	NO	NO
project_lead_user	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
NONE
SHOW GRANTS;
Grants for project_lead_user@%
GRANT USAGE ON *.* TO `project_lead_user`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `project_lead_user`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `project_lead_user`@`%`
GRANT `hr_access`@`%`,`project_lead`@`%` TO `project_lead_user`@`%`
# Enable roles;
SET ROLE hr_access;
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
project_lead_user	%	hr_access	%	hr_rules	%	YES	NO	NO
project_lead_user	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_access`@`%`
SHOW GRANTS;
Grants for project_lead_user@%
GRANT USAGE ON *.* TO `project_lead_user`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `project_lead_user`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger2` TO `project_lead_user`@`%` WITH GRANT OPTION
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `project_lead_user`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `project_lead_user`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `project_lead_user`@`%`
GRANT `hr_access`@`%`,`project_lead`@`%` TO `project_lead_user`@`%`
GRANT `hr_rules`@`%` TO `project_lead_user`@`%` WITH ADMIN OPTION
Disconnect project_lead_user
#
# Case 6: Login as project_lead_user with cyclic role graph.
#
# Form a role graph loop.
#
#              reader
#                |
#                V
#             updater<------------\
#                |                |
#        |``````````````|         |
#        V              V         |
#   task1_lead     task2_lead     |
#        |              |         |
#        ````````|```````         |
#                V                |
#          project_lead-----------/
#
INSERT INTO mysql.role_edges (FROM_USER,FROM_HOST,TO_USER,TO_HOST)
VALUES('project_lead','%','updater','%');
FLUSH PRIVILEGES;
# Connect as project_lead_user
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
project_lead_user	%	hr_access	%	hr_rules	%	YES	NO	NO
project_lead_user	%	project_lead	%	task1_lead	%	NO	NO	NO
project_lead_user	%	project_lead	%	task2_lead	%	NO	NO	NO
project_lead_user	%	project_lead_user	%	hr_access	%	NO	NO	NO
project_lead_user	%	project_lead_user	%	project_lead	%	NO	NO	NO
project_lead_user	%	task1_lead	%	task1	%	NO	NO	NO
project_lead_user	%	task1_lead	%	updater	%	NO	NO	NO
project_lead_user	%	task2_lead	%	task2	%	NO	NO	NO
project_lead_user	%	task2_lead	%	updater	%	NO	NO	NO
project_lead_user	%	updater	%	project_lead	%	NO	NO	NO
project_lead_user	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
NONE
SHOW GRANTS;
Grants for project_lead_user@%
GRANT USAGE ON *.* TO `project_lead_user`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `project_lead_user`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `project_lead_user`@`%`
GRANT `hr_access`@`%`,`project_lead`@`%` TO `project_lead_user`@`%`
# Enable roles;
SET ROLE hr_access;
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
project_lead_user	%	hr_access	%	hr_rules	%	YES	NO	NO
project_lead_user	%	project_lead	%	task1_lead	%	NO	NO	NO
project_lead_user	%	project_lead	%	task2_lead	%	NO	NO	NO
project_lead_user	%	project_lead_user	%	hr_access	%	NO	NO	NO
project_lead_user	%	project_lead_user	%	project_lead	%	NO	NO	NO
project_lead_user	%	task1_lead	%	task1	%	NO	NO	NO
project_lead_user	%	task1_lead	%	updater	%	NO	NO	NO
project_lead_user	%	task2_lead	%	task2	%	NO	NO	NO
project_lead_user	%	task2_lead	%	updater	%	NO	NO	NO
project_lead_user	%	updater	%	project_lead	%	NO	NO	NO
project_lead_user	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_access`@`%`
SHOW GRANTS;
Grants for project_lead_user@%
GRANT USAGE ON *.* TO `project_lead_user`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `project_lead_user`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger2` TO `project_lead_user`@`%` WITH GRANT OPTION
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `project_lead_user`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `project_lead_user`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `project_lead_user`@`%`
GRANT `hr_access`@`%`,`project_lead`@`%` TO `project_lead_user`@`%`
GRANT `hr_rules`@`%` TO `project_lead_user`@`%` WITH ADMIN OPTION
Disconnect project_lead_user
# Connect as project_lead_user
# With the roles activated during connection.
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
project_lead_user	%	hr_access	%	hr_rules	%	YES	NO	NO
project_lead_user	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
NONE
SHOW GRANTS;
Grants for project_lead_user@%
GRANT USAGE ON *.* TO `project_lead_user`@`%`
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `project_lead_user`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `project_lead_user`@`%`
GRANT `hr_access`@`%`,`project_lead`@`%` TO `project_lead_user`@`%`
# Enable roles;
SET ROLE hr_access;
SELECT * FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS ORDER BY GRANTEE, ROLE_NAME;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
project_lead_user	%	hr_access	%	hr_rules	%	YES	NO	NO
project_lead_user	%	updater	%	reader	%	YES	NO	NO
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`hr_access`@`%`
SHOW GRANTS;
Grants for project_lead_user@%
GRANT USAGE ON *.* TO `project_lead_user`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger1` TO `project_lead_user`@`%`
GRANT SELECT, SELECT (`f2`) ON `test`.`hr_ledger2` TO `project_lead_user`@`%` WITH GRANT OPTION
GRANT SELECT, SELECT (`f1`) ON `test`.`t1` TO `project_lead_user`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`proc1` TO `project_lead_user`@`%`
GRANT EXECUTE ON PROCEDURE `test`.`weekly_report` TO `project_lead_user`@`%`
GRANT `hr_access`@`%`,`project_lead`@`%` TO `project_lead_user`@`%`
GRANT `hr_rules`@`%` TO `project_lead_user`@`%` WITH ADMIN OPTION
Disconnect project_lead_user
REVOKE project_lead FROM updater;
#
# Bug#30292102 WL10895 APPLICABLE_ROLES DISPLAYS
# DUPLICATE ENTRY FOR ROLE
#
CREATE USER u1 IDENTIFIED BY 'foo';
CREATE ROLE r1;
GRANT r1 to u1;
GRANT SELECT ON test.* TO u1;
SET GLOBAL mandatory_roles='r1';
GRANT r1 TO u1 WITH ADMIN OPTION;
SET GLOBAL activate_all_roles_on_login = 'ON';
# Should not show duplicate entry for r1->u1 edge after the fix.
SELECT * FROM INFORMATION_SCHEMA.APPLICABLE_ROLES;
USER	HOST	GRANTEE	GRANTEE_HOST	ROLE_NAME	ROLE_HOST	IS_GRANTABLE	IS_DEFAULT	IS_MANDATORY
u1	%	u1	%	r1	%	YES	NO	YES
SET GLOBAL mandatory_roles=default;
SET GLOBAL activate_all_roles_on_login=default;
DROP USER u1;
DROP ROLE r1;
# Cleanup
DROP TABLE task1_ledger1;
DROP TABLE task1_ledger2;
DROP TABLE task2_ledger1;
DROP TABLE task2_ledger2;
DROP PROCEDURE daily_report;
DROP PROCEDURE weekly_report;
DROP PROCEDURE proc1;
DROP TABLE hr_ledger1;
DROP TABLE hr_ledger2;
DROP TABLE t1;
DROP USER user0;
DROP USER user1, user2;
DROP USER lead_user1, lead_user2;
DROP USER project_lead_user;
DROP USER product_lead_user;
DROP ROLE reader, updater;
DROP ROLE task1, task1_lead, task2, task2_lead;
DROP ROLE project_lead;
DROP ROLE product_lead;
DROP ROLE hr_access;
DROP ROLE hr_rules;

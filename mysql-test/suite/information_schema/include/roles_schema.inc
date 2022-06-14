--echo #
--echo # SETUP SCHEMA
--echo #

#
#  Testing INFORMATION_SCHEMA tables for ROLES
#
#  Let us consider following Role dependency graph:
#
#               reader
#                 |
#                 V
#    task1     updater    task2
#      |          |         |         hr_rules
#      |  |``````````````|  |            |
#      V  V              V  V            V
#    task1_lead      task2_lead       hr_access
#         |              |
#         ````````|```````
#                 V
#           project_lead
#                 |
#                 V
#           product_lead
#
#

use test;

--echo #
--echo #  CREATE ROLES
--echo #

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

# Assigned to user and not role, in the test.
CREATE ROLE hr_rules, hr_access;
GRANT hr_rules TO hr_access WITH ADMIN OPTION;

--echo #
--echo # USER ACCOUNT AND ASSIGN ROLE's TO THEM.
--echo #

# user0 has no roles.
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


--echo #
--echo # CREATE DATABASE OBJECTS
--echo #
CREATE TABLE task1_ledger1 (f1 INT, f2 INT);
CREATE TABLE task1_ledger2 (f1 INT, f2 INT);

CREATE TABLE task2_ledger1 (f1 INT, f2 INT);
CREATE TABLE task2_ledger2 (f1 INT, f2 INT);

delimiter //;
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
delimiter ;//

CREATE TABLE hr_ledger1 (f1 INT, f2 INT);
CREATE TABLE hr_ledger2 (f1 INT, f2 INT);

CREATE TABLE t1 (f1 INT);

--echo #
--echo # GRANTS TO ROLE
--echo #

# TABLE GRANT.
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

# Column grant.
GRANT SELECT (f2) ON task1_ledger1 to reader;
GRANT SELECT (f2) ON task1_ledger2 to reader WITH GRANT OPTION;

GRANT SELECT (f2) ON task2_ledger2 to product_lead;
GRANT SELECT (f2) ON task2_ledger2 to product_lead WITH GRANT OPTION;

GRANT SELECT (f2) ON hr_ledger1 TO hr_rules;
GRANT SELECT (f2) ON hr_ledger2 to hr_access WITH GRANT OPTION;

# Routine grant.
GRANT EXECUTE ON PROCEDURE daily_report TO task1_lead;
GRANT EXECUTE ON PROCEDURE weekly_report TO task2_lead;

GRANT ALTER ROUTINE ON PROCEDURE daily_report TO product_lead;
GRANT ALTER ROUTINE ON PROCEDURE weekly_report TO product_lead;

GRANT EXECUTE ON PROCEDURE weekly_report TO hr_rules;
GRANT EXECUTE ON PROCEDURE weekly_report TO hr_access;

# Grants to users are not shown in
# Applicable_roles, Enabled_roles, Role_*_grants I_S tables.
GRANT SELECT ON t1 TO user1, user2, lead_user1, lead_user2, project_lead_user, product_lead_user;
GRANT SELECT (f1) ON t1 TO user1, user2, lead_user1, lead_user2, project_lead_user, product_lead_user;
GRANT EXECUTE ON PROCEDURE proc1 TO user1, user2, lead_user1, lead_user2, project_lead_user, product_lead_user;

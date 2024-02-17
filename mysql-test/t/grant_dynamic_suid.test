--echo #
--echo # Test for WL#15874: Separate privileges for definer object creation
--echo #   and orphan object protection from SET_USER_ID and
--echo #   deprecate SET_USER_ID
--echo #

--echo # FR1: SET_ANY_DEFINER

--let $assert_privilege_name=SET_ANY_DEFINER
--let $assert_privilege_absent=OFF
--source include/assert_dynamic_priv.inc

CREATE USER wl15874@localhost,wl15874_2@localhost;
CREATE DATABASE wl15874;
GRANT TRIGGER,CREATE ROUTINE,CREATE VIEW,EVENT,DROP,CREATE ON wl15874.* TO wl15874@localhost;
GRANT CREATE USER ON *.* TO wl15874@localhost;
--source include/count_sessions.inc
connect(wl15874_con,localhost,wl15874,);
--echo # Connected as wl15874_con
USE wl15874;
--echo # Test FR1: Must fail: create procedure sans SET_ANY_DEFINER
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE DEFINER=wl15874_2@localhost PROCEDURE p1() SELECT 1;
--echo # Test FR1: Must fail: create function sans SET_ANY_DEFINER
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE DEFINER=wl15874_2@localhost FUNCTION f1() RETURNS INT RETURN 12;
--echo # Test FR1: Must fail: create view sans SET_ANY_DEFINER
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE DEFINER=wl15874_2@localhost VIEW v1 AS SELECT 1 as 'a';
CREATE VIEW v1 AS SELECT 1 as 'a';
--echo # Test FR1: Must fail: alter view sans SET_ANY_DEFINER
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER DEFINER=wl15874_2@localhost VIEW v1 AS SELECT 1 as 'a';
DROP VIEW v1;
CREATE TABLE t1(a INT);
--echo # Test FR1: Must fail: create trigger sans SET_ANY_DEFINER
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE DEFINER=wl15874_2@localhost TRIGGER t1_trg1 AFTER INSERT ON t1 FOR EACH ROW SET @sum = @sum + 1;
--echo # Test FR1: Must fail: create event sans SET_ANY_DEFINER
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE DEFINER=wl15874_2@localhost EVENT wl15874_ev1
  ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR
  DO SET @sum = @sum + 1;
CREATE EVENT wl15874_ev1
ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR
DO SET @sum = @sum + 1;
--echo # Test FR1: Must fail: alter event sans SET_ANY_DEFINER
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER DEFINER=wl15874_2@localhost EVENT wl15874_ev1
  ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR
  DO SET @sum = @sum + 2;
DROP EVENT wl15874_ev1;  

--echo NF1.1: must pass: current_user should work.
CREATE DEFINER=CURRENT_USER PROCEDURE p1() SELECT 1;
DROP PROCEDURE p1;

connection default;
disconnect wl15874_con;
GRANT SET_ANY_DEFINER ON *.* TO wl15874@localhost;
connect(wl15874_con,localhost,wl15874,);

--echo # Test FR1: Must pass: create procedure
CREATE DEFINER=wl15874_2@localhost PROCEDURE p1() SELECT 1;
--echo # Test FR1: Must fail: DROP USER to make an orphaned procedure
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
DROP USER wl15874_2@localhost;
DROP PROCEDURE p1;
--echo # Test FR1: Must pass: create function
CREATE DEFINER=wl15874_2@localhost FUNCTION f1() RETURNS INT RETURN 12;
--echo # Test FR1: Must fail: DROP USER to make an orphaned function
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
DROP USER wl15874_2@localhost;
DROP FUNCTION f1;
--echo # Test FR1: Must pass: create view
CREATE DEFINER=wl15874_2@localhost VIEW v1 AS SELECT 1 as 'a';
--echo # Test FR1: Must fail: DROP USER to make an orphaned view
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
DROP USER wl15874_2@localhost;
DROP VIEW v1;
CREATE TABLE t1(a INT);
--echo # Test FR1: Must pass: create trigger
CREATE DEFINER=wl15874_2@localhost TRIGGER t1_trg1 AFTER INSERT ON t1 FOR EACH ROW SET @sum = @sum + 1;
--echo # Test FR1: Must fail: DROP USER to make an orphaned trigger
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
DROP USER wl15874_2@localhost;
DROP TRIGGER t1_trg1;
DROP TABLE t1;
CREATE VIEW v1 AS SELECT 1 as 'a';
--echo # Test FR1: Must pass: alter view
ALTER DEFINER=wl15874_2@localhost VIEW v1 AS SELECT 2 as 'a';
DROP VIEW v1;
--echo # Test FR1: Must pass: create event
CREATE DEFINER=wl15874_2@localhost EVENT wl15874_ev1
  ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR
  DO SET @sum = @sum + 1;
--echo # Test FR1: Must fail: DROP USER to make an orphaned event
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
DROP USER wl15874_2@localhost;
--echo # Test FR1: Must pass: alter event
ALTER DEFINER=wl15874_2@localhost EVENT wl15874_ev1
  ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR
  DO SET @sum = @sum + 2;
DROP EVENT wl15874_ev1;

--echo # FR2: ALLOW_NONEXISTENT_DEFINER

connection default;
--let $assert_privilege_name=ALLOW_NONEXISTENT_DEFINER
--let $assert_privilege_absent=OFF
--source include/assert_dynamic_priv.inc

disconnect wl15874_con;
REVOKE SET_ANY_DEFINER ON *.* FROM wl15874@localhost;
GRANT ALLOW_NONEXISTENT_DEFINER ON *.* TO wl15874@localhost;
connect(wl15874_con,localhost,wl15874,);

--echo # FR2: must fail: ALLOW_NONEXISTENT_DEFINER is not enough by itself
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE DEFINER=wl15874_na@localhost PROCEDURE p1() SELECT 1;

connection default;
disconnect wl15874_con;
GRANT SET_ANY_DEFINER ON *.* TO wl15874@localhost;
connect(wl15874_con,localhost,wl15874,);

--echo # Test FR1: Must pass: create procedure non-existent
CREATE DEFINER=wl15874_na@localhost PROCEDURE p1() SELECT 1;
--echo # Test FR1: Must pass: create function non-existent
CREATE DEFINER=wl15874_na@localhost FUNCTION f1() RETURNS INT RETURN 12;
--echo # Test FR1: Must pass: create view non-existent
CREATE DEFINER=wl15874_na@localhost VIEW v1 AS SELECT 1 as 'a';
CREATE TABLE t1(a INT);
--echo # Test FR1: Must pass: create trigger non-existent
CREATE DEFINER=wl15874_na@localhost TRIGGER t1_trg1 AFTER INSERT ON t1 FOR EACH ROW SET @sum = @sum + 1;
--echo # Test FR1: Must pass: alter view non-existent
ALTER DEFINER=wl15874_na@localhost VIEW v1 AS SELECT 2 as 'a';
--echo # Test FR1: Must pass: create event non-existent
CREATE DEFINER=wl15874_na@localhost EVENT wl15874_ev1
  ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR
  DO SET @sum = @sum + 1;
--echo # Test FR1: Must pass: alter event non-existent
ALTER DEFINER=wl15874_na@localhost EVENT wl15874_ev1
  ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR
  DO SET @sum = @sum + 2;
--echo # Test FR1: addoption of orphaned objects via CREATE USER: must pass
CREATE USER wl15874_na@localhost;
--echo # Test FR1: making them orphaned again via DROP USER: must pass.
DROP USER wl15874_na@localhost;
CREATE USER wl15874_na@localhost;
--echo # Test FR1: making them orphaned again via RENAME USER: must pass.
RENAME USER wl15874_na@localhost TO wl15874_na_ren@localhost;
DROP USER wl15874_na_ren@localhost;
--echo # Test FR1: addoption of orphaned objects via CREATE ROLE: must pass
CREATE ROLE wl15874_na@localhost;
--echo # Test FR1: making them orphaned again via DROP ROLE: must pass.
DROP ROLE wl15874_na@localhost;
CREATE ROLE wl15874_na@localhost;
--echo # Test FR1: making them orphaned again via RENAME USER for role: must pass.
RENAME USER wl15874_na@localhost TO wl15874_na_ren@localhost;
DROP ROLE wl15874_na_ren@localhost;

--echo # cleanup
DROP PROCEDURE p1;
DROP FUNCTION f1;
DROP VIEW v1;
DROP TRIGGER t1_trg1;
DROP TABLE t1;
DROP EVENT wl15874_ev1;

connection default;
disconnect wl15874_con;
REVOKE ALLOW_NONEXISTENT_DEFINER ON *.* FROM wl15874@localhost;
connect(wl15874_con,localhost,wl15874,);
--echo # Test FR2.2: Must fail: create procedure non-existent sans ALLOW_NONEXISTENT_DEFINER
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE DEFINER=wl15874_na@localhost PROCEDURE p1() SELECT 1;
--echo # Test FR2.2: Must fail: create function non-existent sans ALLOW_NONEXISTENT_DEFINER
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE DEFINER=wl15874_na@localhost FUNCTION f1() RETURNS INT RETURN 12;
--echo # Test FR2.2: Must fail: create view non-existent sans ALLOW_NONEXISTENT_DEFINER
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE DEFINER=wl15874_na@localhost VIEW v1 AS SELECT 1 as 'a';
CREATE TABLE t1(a INT);
--echo # Test FR2.2: Must fail: create trigger non-existent sans ALLOW_NONEXISTENT_DEFINER
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE DEFINER=wl15874_na@localhost TRIGGER t1_trg1 AFTER INSERT ON t1 FOR EACH ROW SET @sum = @sum + 1;

DROP TABLE t1;

CREATE VIEW v1 as SELECT 3 as 'a';
--echo # Test FR2.2: Must fail: alter view non-existent sans ALLOW_NONEXISTENT_DEFINER
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER DEFINER=wl15874_na@localhost VIEW v1 AS SELECT 2 as 'a';

DROP VIEW v1;

--echo # Test FR2.2: Must fail: create event non-existent sans ALLOW_NONEXISTENT_DEFINER
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE DEFINER=wl15874_na@localhost EVENT wl15874_ev1
  ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR
  DO SET @sum = @sum + 1;


CREATE EVENT wl15874_ev1
  ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR
  DO SET @sum = @sum + 1;
--echo # Test FR2.2: Must fail: alter event non-existent sans ALLOW_NONEXISTENT_DEFINER
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER DEFINER=wl15874_na@localhost EVENT wl15874_ev1
  ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR
  DO SET @sum = @sum + 2;

DROP EVENT wl15874_ev1;

--echo # global cleanup
connection default;
disconnect wl15874_con;
--source include/wait_until_count_sessions.inc
REVOKE TRIGGER,CREATE ROUTINE,CREATE VIEW,EVENT,DROP,CREATE ON wl15874.* FROM wl15874@localhost;
REVOKE CREATE USER ON *.* FROM wl15874@localhost;
REVOKE SET_ANY_DEFINER ON *.* FROM wl15874@localhost;
DROP DATABASE wl15874;
DROP USER wl15874@localhost,wl15874_2@localhost;

--echo # End of 8.2 tests

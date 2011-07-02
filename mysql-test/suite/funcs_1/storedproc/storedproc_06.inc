#### suite/funcs_1/storedproc/storedproc_06.inc

# This test cannot be used for the embedded server because we check here
# privileges.
--source include/not_embedded.inc

--source suite/funcs_1/storedproc/load_sp_tb.inc

# ==============================================================================
# (numbering from requirement document TP v1.0, Last updated: 25 Jan 2005 01:00)
#
# 3.1.6 Privilege checks:
#
#  1. Ensure that no user may create a stored procedure without the
#     GRANT CREATE ROUTINE privilege.
#  2. Ensure that root always has the GRANT CREATE ROUTINE privilege.
#  3. Ensure that a user with the GRANT CREATE ROUTINE privilege can always
#     create both a procedure and a function, on any appropriate database.
#  4. Ensure that the default security provision of a stored procedure is
#     SQL SECURITY DEFINER.
#  5. Ensure that a stored procedure defined with SQL SECURITY DEFINER can be
#     called/executed by any user, using only the privileges (including
#     database access privileges) associated with the user who created
#     the stored procedure.
#  6. Ensure that a stored procedure defined with SQL SECURITY INVOKER can be
#     called/executed by any user, using only the privileges (including
#     database access privileges) associated with the user executing
#     the stored procedure.
#
# ==============================================================================
let $message= Section 3.1.6 - Privilege Checks:;
--source include/show_msg80.inc


connection default;
USE db_storedproc_1;
--source suite/funcs_1/include/show_connection.inc

# ------------------------------------------------------------------------------
let $message= Testcase 3.1.6.1:
              -----------------
Ensure that no user may create a stored procedure without the GRANT CREATE
ROUTINE privilege.;
--source include/show_msg80.inc

create user 'user_1'@'localhost';

grant all on db_storedproc_1.* to 'user_1'@'localhost';
revoke create routine on db_storedproc_1.* from 'user_1'@'localhost';
flush privileges;

--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
--enable_warnings

connect (user1a, localhost, user_1, , db_storedproc_1);
--source suite/funcs_1/include/show_connection.inc

USE db_storedproc_1;

delimiter //;
--error ER_DBACCESS_DENIED_ERROR
CREATE PROCEDURE sp1(v1 char(20))
BEGIN
    SELECT * from db_storedproc_1.t6 where t6.f2= 'xyz';
END//
delimiter ;//

disconnect user1a;

# add privilege again and check
connection default;
USE db_storedproc_1;
--source suite/funcs_1/include/show_connection.inc

GRANT CREATE ROUTINE ON db_storedproc_1.* TO 'user_1'@'localhost';
connect (user1b, localhost, user_1, , db_storedproc_1);
--source suite/funcs_1/include/show_connection.inc

USE db_storedproc_1;

delimiter //;
CREATE PROCEDURE sp1(v1 char(20))
BEGIN
    SELECT * from db_storedproc_1.t6 where t6.f2= 'xyz';
END//
delimiter ;//
disconnect user1b;

# cleanup
connection default;
USE db_storedproc_1;
--source suite/funcs_1/include/show_connection.inc

DROP USER 'user_1'@'localhost';
DROP PROCEDURE sp1;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.6.2:
              -----------------
Ensure that root always has the GRANT CREATE ROUTINE privilege.
(checked by other testscases);
--source include/show_msg80.inc


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.6.3:
              -----------------
Ensure that a user with the GRANT CREATE ROUTINE privilege can always create
both a procedure and a function, on any appropriate database.
--source include/show_msg80.inc


create user 'user_1'@'localhost';

grant create routine on db_storedproc_1.* to 'user_1'@'localhost';
flush privileges;

--disable_warnings
DROP PROCEDURE IF EXISTS db_storedproc_1.sp3;
DROP FUNCTION IF EXISTS db_storedproc_1.fn1;
--enable_warnings

#  disconnect default;
connect (user2, localhost, user_1, , db_storedproc_1);
--source suite/funcs_1/include/show_connection.inc

delimiter //;
CREATE PROCEDURE sp3(v1 char(20))
BEGIN
   SELECT * from db_storedproc_1.t6 where t6.f2= 'xyz';
END//
delimiter ;//

delimiter //;
CREATE FUNCTION fn1(v1 int) returns int
BEGIN
   return v1;
END//
delimiter ;//

disconnect user2;

# cleanup
connection default;
USE db_storedproc_1;
--source suite/funcs_1/include/show_connection.inc

drop user 'user_1'@'localhost';
DROP PROCEDURE sp3;
DROP FUNCTION fn1;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.6.4:
              -----------------
Ensure that the default security provision of a stored procedure is SQL SECURITY
DEFINER.;
--source include/show_msg80.inc

CREATE USER 'user_1'@'localhost';

grant update on db_storedproc_1.t6 to 'user_1'@'localhost';
grant execute on db_storedproc_1.* to 'user_1'@'localhost';
flush privileges;

USE db_storedproc_1;

--disable_warnings
DROP PROCEDURE IF EXISTS sp4;
--enable_warnings

delimiter //;
CREATE PROCEDURE sp4(v1 char(20))
BEGIN
   SELECT * from db_storedproc_1.t6 where t6.f2= 'xyz';
END//
delimiter ;//

#disconnect default;

connect (user3, localhost, user_1, , db_storedproc_1);
--source suite/funcs_1/include/show_connection.inc

USE db_storedproc_1;
CALL sp4('a');

--vertical_results
SELECT SPECIFIC_NAME, ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE,
       ROUTINE_BODY, ROUTINE_DEFINITION, IS_DETERMINISTIC,
       SQL_DATA_ACCESS, SECURITY_TYPE, SQL_MODE, ROUTINE_COMMENT
FROM information_schema.routines
 WHERE routine_schema LIKE 'db_sto%';
--horizontal_results

disconnect user3;

# cleanup
connection default;
--source suite/funcs_1/include/show_connection.inc
DROP PROCEDURE sp4;
DROP USER 'user_1'@'localhost';


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.6.5:
              -----------------
Ensure that a stored procedure defined with SQL SECURITY DEFINER can be
called/executed by any user, using only the privileges (including database
access privileges) associated with the user who created the stored procedure.;
--source include/show_msg80.inc

USE db_storedproc_1;
CREATE TABLE t3165 ( c1 char(20), c2 char(20), c3 date);
INSERT INTO t3165 VALUES ('inserted', 'outside of SP', NULL);

# creates procedures
create user 'user_1'@'localhost';

#executes procedure
create user 'user_2'@'localhost';

grant create routine on db_storedproc_1.* to 'user_1'@'localhost';
grant SELECT on db_storedproc_1.* to 'user_2'@'localhost';
grant execute on db_storedproc_1.* to 'user_2'@'localhost';
flush privileges;

connect (user5_1, localhost, user_1, , db_storedproc_1);
--source suite/funcs_1/include/show_connection.inc

delimiter //;
CREATE PROCEDURE sp5_s_i () sql security definer
BEGIN
   SELECT * from db_storedproc_1.t3165;
   insert into db_storedproc_1.t3165 values ('inserted', 'from sp5_s_i', 1000);
END//

CREATE PROCEDURE sp5_sel () sql security definer
BEGIN
   SELECT * from db_storedproc_1.t3165;
END//

CREATE PROCEDURE sp5_ins () sql security definer
BEGIN
   insert into db_storedproc_1.t3165 values ('inserted', 'from sp5_ins', 1000);
END//
delimiter ;//

disconnect user5_1;

connect (user5_2, localhost, user_2, , db_storedproc_1);
--source suite/funcs_1/include/show_connection.inc

--error ER_TABLEACCESS_DENIED_ERROR
CALL sp5_s_i();
--error ER_TABLEACCESS_DENIED_ERROR
CALL sp5_ins();
--error ER_TABLEACCESS_DENIED_ERROR
CALL sp5_sel();

# now 'add' INSERT to DEFINER
connection default;
--source suite/funcs_1/include/show_connection.inc
--error ER_TABLEACCESS_DENIED_ERROR
CALL sp5_sel();
grant insert on db_storedproc_1.* to 'user_1'@'localhost';
flush privileges;

connection user5_2;
--source suite/funcs_1/include/show_connection.inc
--error ER_TABLEACCESS_DENIED_ERROR
CALL sp5_s_i();
CALL sp5_ins();
--error ER_TABLEACCESS_DENIED_ERROR
CALL sp5_sel();

# now 'add' SELECT to DEFINER
connection default;
--source suite/funcs_1/include/show_connection.inc
--error ER_TABLEACCESS_DENIED_ERROR
CALL sp5_sel();
grant SELECT on db_storedproc_1.* to 'user_1'@'localhost';
#grant execute on db_storedproc_1.* to 'user_2'@'localhost';
flush privileges;

connection user5_2;
--source suite/funcs_1/include/show_connection.inc
CALL sp5_s_i();
CALL sp5_ins();
CALL sp5_sel();

# now revoke INSERT FROM DEFINER
connection default;
--source suite/funcs_1/include/show_connection.inc
REVOKE INSERT on db_storedproc_1.* from 'user_1'@'localhost';
flush privileges;

connection user5_2;
--source suite/funcs_1/include/show_connection.inc
--error ER_TABLEACCESS_DENIED_ERROR
CALL sp5_s_i();
--error ER_TABLEACCESS_DENIED_ERROR
CALL sp5_ins();
CALL sp5_sel();

# now revoke SELECT FROM DEFINER
connection default;
--source suite/funcs_1/include/show_connection.inc
REVOKE SELECT on db_storedproc_1.* from 'user_1'@'localhost';
flush privileges;

connection user5_2;
--source suite/funcs_1/include/show_connection.inc
--error ER_TABLEACCESS_DENIED_ERROR
CALL sp5_s_i();
--error ER_TABLEACCESS_DENIED_ERROR
CALL sp5_ins();
--error ER_TABLEACCESS_DENIED_ERROR
CALL sp5_sel();

# cleanup
disconnect user5_2;
connection default;
--source suite/funcs_1/include/show_connection.inc

DROP PROCEDURE sp5_s_i;
DROP PROCEDURE sp5_sel;
DROP PROCEDURE sp5_ins;
DROP TABLE t3165;
DROP USER 'user_1'@'localhost';
DROP USER 'user_2'@'localhost';


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.6.6:
              -----------------
Ensure that a stored procedure defined with SQL SECURITY INVOKER can be
called/executed by any user, using only the privileges (including database
access privileges) associated with the user executing the stored procedure.;
--source include/show_msg80.inc

USE db_storedproc_1;
CREATE TABLE t3166 ( c1 char(30) );
INSERT INTO db_storedproc_1.t3166 VALUES ('inserted outside SP');

# DEFINER
create user 'user_1'@'localhost';

# INVOKER
create user 'user_2'@'localhost';

GRANT CREATE ROUTINE ON db_storedproc_1.* TO 'user_1'@'localhost';
GRANT SELECT  ON db_storedproc_1.* TO 'user_2'@'localhost';
GRANT EXECUTE ON db_storedproc_1.* TO 'user_2'@'localhost';
FLUSH PRIVILEGES;

connect (user6_1, localhost, user_1, , db_storedproc_1);
--source suite/funcs_1/include/show_connection.inc

delimiter //;
CREATE PROCEDURE sp3166_s_i () SQL SECURITY INVOKER
BEGIN
   SELECT * from db_storedproc_1.t3166;
   insert into db_storedproc_1.t3166 values ('inserted from sp3166_s_i');
END//

CREATE PROCEDURE sp3166_sel () SQL SECURITY INVOKER
BEGIN
   SELECT * from db_storedproc_1.t3166;
END//

CREATE PROCEDURE sp3166_ins () SQL SECURITY INVOKER
BEGIN
   insert into db_storedproc_1.t3166 values ('inserted from sp3166_ins');
END//
delimiter ;//

disconnect user6_1;

connect (user6_2, localhost, user_2, , db_storedproc_1);
--source suite/funcs_1/include/show_connection.inc

--error ER_TABLEACCESS_DENIED_ERROR
CALL sp3166_s_i();
--error ER_TABLEACCESS_DENIED_ERROR
CALL sp3166_ins();
CALL sp3166_sel();

# now 'add' INSERT to INVOKER
connection default;
--source suite/funcs_1/include/show_connection.inc
CALL sp3166_sel();
GRANT INSERT  ON db_storedproc_1.* TO 'user_2'@'localhost';
FLUSH PRIVILEGES;
disconnect user6_2;

connect (user6_3, localhost, user_2, , db_storedproc_1);
--source suite/funcs_1/include/show_connection.inc
CALL sp3166_s_i();
CALL sp3166_ins();
CALL sp3166_sel();
disconnect user6_3;

# now 'remove' SELECT from INVOKER
connection default;
--source suite/funcs_1/include/show_connection.inc
CALL sp3166_sel();
REVOKE SELECT ON db_storedproc_1.* FROM 'user_2'@'localhost';
FLUSH PRIVILEGES;

connect (user6_4, localhost, user_2, , db_storedproc_1);
--source suite/funcs_1/include/show_connection.inc
--error ER_TABLEACCESS_DENIED_ERROR
CALL sp3166_s_i();
CALL sp3166_ins();
--error ER_TABLEACCESS_DENIED_ERROR
CALL sp3166_sel();
disconnect user6_4;

# now 'remove' EXECUTE FROM INVOKER
connection default;
CALL sp3166_s_i();
--source suite/funcs_1/include/show_connection.inc
REVOKE EXECUTE on db_storedproc_1.* FROM 'user_2'@'localhost';
FLUSH PRIVILEGES;

connect (user6_5, localhost, user_2, , db_storedproc_1);
--source suite/funcs_1/include/show_connection.inc
--error ER_PROCACCESS_DENIED_ERROR
CALL sp3166_s_i();
--error ER_PROCACCESS_DENIED_ERROR
CALL sp3166_ins();
--error ER_PROCACCESS_DENIED_ERROR
CALL sp3166_sel();
disconnect user6_5;

# cleanup
connection default;
--source suite/funcs_1/include/show_connection.inc

DROP PROCEDURE sp3166_s_i;
DROP PROCEDURE sp3166_sel;
DROP PROCEDURE sp3166_ins;
DROP TABLE t3166;
DROP USER 'user_1'@'localhost';
DROP USER 'user_2'@'localhost';


# ==============================================================================
# USE the same .inc to cleanup before and after the test
--source suite/funcs_1/storedproc/cleanup_sp_tb.inc

# ==============================================================================
--echo
--echo .                               +++ END OF SCRIPT +++
--echo --------------------------------------------------------------------------------
# ==============================================================================

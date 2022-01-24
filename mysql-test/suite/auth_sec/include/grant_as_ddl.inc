###############################################################################
#  This script executes GRANT AS scenarios                                    #
###############################################################################

--echo -------------------------------------------------------------------------
--echo # 1. GRANT AS sanity checks
--echo -------------------------------------------------------------------------

CREATE USER foo, bar, baz;
CREATE DATABASE db1;
CREATE TABLE db1.t1(c1 int);

--error ER_UNSUPPORTED_USE_OF_GRANT_AS
GRANT SELECT ON db1.* TO foo AS bar;

--error ER_UNSUPPORTED_USE_OF_GRANT_AS
GRANT SELECT ON db1.t1 TO foo AS bar;

--error ER_UNSUPPORTED_USE_OF_GRANT_AS
GRANT SELECT(c1) ON db1.t1 TO foo AS bar;

--error ER_PARSE_ERROR
GRANT PROXY ON foo TO bar AS foo;

--error ER_PARSE_ERROR
GRANT baz TO bar AS foo;

--error ER_PARSE_ERROR
GRANT SELECT ON *.* TO bar WITH ROLE foo;

--error ER_PARSE_ERROR
GRANT SELECT ON *.* TO bar WITH ROLE NONE;

--error ER_PARSE_ERROR
GRANT SELECT ON *.* TO bar WITH ROLE ALL;

--error ER_PARSE_ERROR
GRANT SELECT ON *.* TO bar WITH ROLE DEFAULT;

--error ER_UKNOWN_AUTH_ID_OR_ACCESS_DENIED_FOR_GRANT_AS
GRANT SELECT ON *.* TO foo AS hohoho;

--error ER_UKNOWN_AUTH_ID_OR_ACCESS_DENIED_FOR_GRANT_AS
GRANT SELECT ON *.* TO foo AS bar WITH ROLE hohoho;

--error ER_UKNOWN_AUTH_ID_OR_ACCESS_DENIED_FOR_GRANT_AS
GRANT SELECT ON *.* TO foo AS bar WITH ROLE baz, hohoho;

DROP TABLE db1.t1;
DROP DATABASE db1;
DROP USER foo, bar, baz;

--echo -------------------------------------------------------------------------
--echo # 2. Permission checks
--echo -------------------------------------------------------------------------

CREATE USER foo, bar;
CREATE ROLE baz, qux;
CREATE DATABASE db1;

--error ER_UKNOWN_AUTH_ID_OR_ACCESS_DENIED_FOR_GRANT_AS
GRANT SELECT ON *.* TO foo AS bar WITH ROLE baz;

GRANT SELECT ON *.* TO foo AS `root`@`localhost`;

GRANT SELECT ON *.* TO foo WITH GRANT OPTION;
REVOKE SELECT ON db1.* FROM foo;

--connect(conn_foo, localhost, foo,,,,,)

# Impersonating a user more privileged than you is not allowed
--error ER_UKNOWN_AUTH_ID_OR_ACCESS_DENIED_FOR_GRANT_AS
GRANT SELECT ON *.* TO bar AS `root`@`localhost`;

--connection default
GRANT SYSTEM_USER ON *.* TO foo;

--connection conn_foo
# Impersonating a user more privileged than you is not allowed
--error ER_UKNOWN_AUTH_ID_OR_ACCESS_DENIED_FOR_GRANT_AS
GRANT SELECT ON *.* TO bar AS `root`@`localhost`;

--connection default
--disconnect conn_foo
REVOKE ALL ON *.* FROM foo;
GRANT SYSTEM_USER ON *.* TO foo;
GRANT INSERT ON *.* TO foo WITH GRANT OPTION;
GRANT SELECT ON *.* TO baz WITH GRANT OPTION;
GRANT SELECT ON db1.* to qux;
GRANT baz, qux TO foo;

--connect(conn_foo, localhost, foo,,,,,)
SET ROLE baz;
GRANT SELECT ON *.* TO bar AS `root`@`localhost`;

SET ROLE qux;
--error ER_ACCESS_DENIED_ERROR
GRANT SELECT ON *.* TO bar;

--connection default
--disconnect conn_foo

DROP DATABASE db1;
DROP ROLE baz, qux;
DROP USER foo, bar;

CREATE USER foo, bar, baz;
CREATE ROLE r1, r2, r3;
GRANT SELECT ON *.* TO foo WITH GRANT OPTION;
GRANT SYSTEM_USER ON *.* TO r2;
GRANT r2 TO r3;
GRANT r1, r3 TO bar;

--connect(conn_foo, localhost, foo,,,,,)
# Should succeed
GRANT SELECT ON *.* TO baz AS bar;
GRANT SELECT ON *.* TO baz AS bar WITH ROLE NONE;
GRANT SELECT ON *.* TO baz AS bar WITH ROLE r1;

# Impersonating a user more privileged than you is not allowed
--error ER_UKNOWN_AUTH_ID_OR_ACCESS_DENIED_FOR_GRANT_AS
GRANT SELECT ON *.* TO baz AS bar WITH ROLE r2;
--error ER_UKNOWN_AUTH_ID_OR_ACCESS_DENIED_FOR_GRANT_AS
GRANT SELECT ON *.* TO baz AS bar WITH ROLE r1, r2;

GRANT SELECT ON *.* TO baz AS bar WITH ROLE ALL;

--connection default
--disconnect conn_foo
SHOW GRANTS FOR baz;

DROP ROLE r1, r2, r3;
DROP USER foo, bar, baz;

--echo -------------------------------------------------------------------------
--echo # 3. User test
--echo -------------------------------------------------------------------------

CREATE USER foo, bar;
CREATE DATABASE db1;

GRANT SELECT, INSERT ON *.* TO bar AS CURRENT_USER();
SHOW GRANTS FOR bar;

REVOKE ALL ON *.* FROM bar;
GRANT SELECT ON *.* TO bar AS foo;
SHOW GRANTS FOR bar;

REVOKE ALL ON *.* FROM bar;
GRANT SELECT ON *.* TO foo;
REVOKE SELECT ON db1.* FROM foo;

GRANT SELECT ON *.* TO bar AS foo;
SHOW GRANTS FOR bar;

REVOKE ALL ON *.* FROM bar;
GRANT SELECT, INSERT ON *.* TO bar AS foo;
SHOW GRANTS FOR bar;

DROP DATABASE db1;
DROP USER foo, bar;

--echo -------------------------------------------------------------------------
--echo # 4. Roles test
--echo -------------------------------------------------------------------------

CREATE USER foo, bar;
CREATE ROLE r1, r2, r3, r4;
CREATE DATABASE db1;

GRANT SELECT ON *.* TO r1;
REVOKE SELECT ON db1.* FROM r1;
GRANT r1 TO r2;

GRANT INSERT ON *.* TO r3;
REVOKE INSERT ON db1.* FROM r3;
GRANT r3 TO r4;

GRANT r2, r4 TO foo;

GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE ALL;
SHOW GRANTS FOR bar;

REVOKE ALL ON *.* FROM bar;
ALTER USER foo DEFAULT ROLE ALL;

GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE ALL;
SHOW GRANTS FOR bar;

REVOKE ALL ON *.* FROM bar;

GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE ALL EXCEPT r4;
SHOW GRANTS FOR bar;

REVOKE ALL ON *.* FROM bar;
ALTER USER foo DEFAULT ROLE ALL;

GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE r2, r4;
SHOW GRANTS FOR bar;

DROP DATABASE db1;
DROP ROLE r1, r2, r3, r4;
DROP USER foo, bar;

--echo -------------------------------------------------------------------------
--echo # 6. Without --partial_revokes
--echo -------------------------------------------------------------------------

SET @save_partial_revokes = @@global.partial_revokes;
SET @@global.partial_revokes = OFF;

CREATE USER foo, bar;
CREATE ROLE r1, r2;

GRANT r1, r2 TO foo;
ALTER USER foo DEFAULT ROLE ALL;

GRANT SELECT, INSERT ON *.* TO bar AS foo;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE NONE;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE ALL;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE ALL EXCEPT r1;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE DEFAULT;

DROP USER foo, bar;
DROP ROLE r1, r2;

SET @@global.partial_revokes = @save_partial_revokes;

--echo -------------------------------------------------------------------------
--echo # 7. Binlog tests
--echo -------------------------------------------------------------------------

--let $mask_anonymous_gtid_events= 1
--let $mask_user_password_events= 1

CREATE USER foo, bar;
CREATE ROLE r1, r2, r3, r4;
CREATE DATABASE db1;

GRANT SELECT ON *.* TO r1 WITH GRANT OPTION;
REVOKE SELECT ON db1.* FROM r1;
GRANT r1 TO r2;

GRANT INSERT ON *.* TO r3 WITH GRANT OPTION;
REVOKE INSERT ON db1.* FROM r3;
GRANT r3 TO r4;

GRANT r2, r4 TO foo;

ALTER USER foo DEFAULT ROLE ALL;

--source include/save_binlog_position.inc
--let $binlog_start = $binlog_position

GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE ALL;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE NONE;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE ALL EXCEPT r2;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE DEFAULT;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE r2, r4;

GRANT SELECT, INSERT ON *.* TO bar AS CURRENT_USER();
GRANT SELECT, INSERT ON *.* TO bar AS CURRENT_USER() WITH ROLE ALL;
GRANT SELECT, INSERT ON *.* TO bar AS CURRENT_USER() WITH ROLE NONE;
GRANT SELECT, INSERT ON *.* TO bar AS CURRENT_USER() WITH ROLE DEFAULT;

GRANT SELECT, INSERT ON *.* TO foo AS foo WITH ROLE ALL;
GRANT SELECT, INSERT ON *.* TO foo AS foo WITH ROLE NONE;
GRANT SELECT, INSERT ON *.* TO foo AS foo WITH ROLE ALL EXCEPT r2;
GRANT SELECT, INSERT ON *.* TO foo AS foo WITH ROLE DEFAULT;
GRANT SELECT, INSERT ON *.* TO foo AS foo WITH ROLE r2, r4;

--connect(conn_foo, localhost, foo,,,,,)

GRANT SELECT, INSERT ON *.* TO bar;

--connection default
--disconnect conn_foo
--source include/show_binlog_events.inc

GRANT SELECT, INSERT ON *.* TO foo;
REVOKE SELECT, INSERT ON db1.* FROM foo;

--source include/save_binlog_position.inc
--let $binlog_start = $binlog_position

GRANT SELECT, INSERT ON *.* TO bar AS foo;

--source include/show_binlog_events.inc

DROP DATABASE db1;
DROP ROLE r1, r2, r3, r4;
DROP USER foo, bar;

--echo -------------------------------------------------------------------------
--echo # 8. Current user in AS caluse
--echo -------------------------------------------------------------------------

CREATE USER foo, bar, baz;
CREATE ROLE r1, r2;
GRANT SELECT, INSERT ON *.* TO r1 WITH GRANT OPTION;
REVOKE SELECT ON mysql.* FROM r1;
GRANT SELECT, INSERT ON *.* TO r2 WITH GRANT OPTION;
REVOKE INSERT ON mysql.* FROM r2;
GRANT r1, r2 TO foo;
ALTER USER foo DEFAULT ROLE ALL;

--connect(conn_foo, localhost, foo,,,,,)

GRANT SELECT ON *.* TO bar AS current_user() WITH ROLE r1;

GRANT INSERT ON *.* TO baz AS foo WITH ROLE r2;

--connection default
--disconnect conn_foo
SHOW GRANTS FOR bar;
SHOW GRANTS FOR baz;

DROP ROLE r1, r2;
DROP USER foo, bar, baz;

CREATE USER foo, bar;
CREATE ROLE r1, r2;
GRANT r1, r2 TO foo;
GRANT SELECT ON *.* TO r1 WITH GRANT OPTION;
REVOKE SELECT ON mysql.* FROM r1;
GRANT INSERT ON *.* TO r2 WITH GRANT OPTION;
REVOKE INSERT ON mysql.* FROM r2;
ALTER USER foo DEFAULT ROLE ALL;

--connect(conn_foo, localhost, foo,,,,,)

--error ER_UKNOWN_AUTH_ID_OR_ACCESS_DENIED_FOR_GRANT_AS
GRANT SELECT, INSERT ON *.* TO bar AS current_user();
--error ER_UKNOWN_AUTH_ID_OR_ACCESS_DENIED_FOR_GRANT_AS
GRANT SELECT, INSERT ON *.* TO bar AS current_user() WITH ROLE r1;
--error ER_UKNOWN_AUTH_ID_OR_ACCESS_DENIED_FOR_GRANT_AS
GRANT SELECT, INSERT ON *.* TO bar AS current_user() WITH ROLE r2;

--error ER_UKNOWN_AUTH_ID_OR_ACCESS_DENIED_FOR_GRANT_AS
GRANT SELECT, INSERT ON *.* TO bar AS foo;
--error ER_UKNOWN_AUTH_ID_OR_ACCESS_DENIED_FOR_GRANT_AS
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE r1;
--error ER_UKNOWN_AUTH_ID_OR_ACCESS_DENIED_FOR_GRANT_AS
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE r2;

--connection default
--disconnect conn_foo
DROP ROLE r1, r2;
DROP USER foo, bar;

--echo -------------------------------------------------------------------------
--echo # 9. General log
--echo -------------------------------------------------------------------------

SET @save_partial_revokes = @@global.partial_revokes;

CREATE USER u1, u2;
CREATE ROLE r1, r2;
GRANT r1, r2 TO u1;
GRANT SELECT ON *.* TO r1 WITH GRANT OPTION;
GRANT INSERT ON *.* TO r2 WITH GRANT OPTION;
ALTER USER u1 DEFAULT ROLE ALL;
--connect(conn_u1, localhost, u1,,,,,)

--connection default

# Make sure we start with a clean slate. log_tables.test says this is OK.
TRUNCATE TABLE mysql.general_log;
SET @old_log_output= @@global.log_output;
SET @old_general_log= @@global.general_log;
SET @old_general_log_file= @@global.general_log_file;

let $general_file_off = $MYSQLTEST_VARDIR/log/grant_as_ddl_general.log;
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
eval SET GLOBAL general_log_file = '$general_file_off';
SET GLOBAL log_output = 'FILE,TABLE';
SET GLOBAL general_log= 'ON';

SET @@global.partial_revokes = ON;

REVOKE SELECT ON mysql.* FROM r1;
REVOKE INSERT ON mysql.* FROM r2;

GRANT SELECT, INSERT ON *.* TO u2;
GRANT SELECT, INSERT ON *.* TO u2 AS u1;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE NONE;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE DEFAULT;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE ALL;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE ALL EXCEPT r1;

--connection conn_u1
SET ROLE NONE;

SET ROLE r1;
GRANT SELECT ON *.* TO u2;

SET ROLE r1, r2;
GRANT SELECT, INSERT ON *.* TO u2;

SET ROLE ALL;
GRANT SELECT, INSERT ON *.* TO u2;

SET ROLE DEFAULT;
GRANT SELECT, INSERT ON *.* TO u2;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE r1;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE ALL;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE DEFAULT;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE ALL EXCEPT r2;

SET ROLE NONE;

--connection default
GRANT SELECT ON *.* TO r1;
GRANT INSERT ON *.* TO r2;

SET @@global.partial_revokes = OFF;

GRANT SELECT, INSERT ON *.* TO u2;
GRANT SELECT, INSERT ON *.* TO u2 AS u1;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE NONE;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE DEFAULT;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE ALL;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE ALL EXCEPT r1;

--connection conn_u1
SET ROLE NONE;

SET ROLE r1;
GRANT SELECT ON *.* TO u2;

SET ROLE r1, r2;
GRANT SELECT, INSERT ON *.* TO u2;

SET ROLE ALL;
GRANT SELECT, INSERT ON *.* TO u2;

SET ROLE DEFAULT;
GRANT SELECT, INSERT ON *.* TO u2;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE r1;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE ALL;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE DEFAULT;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE ALL EXCEPT r2;

SET ROLE NONE;

--connection default
--disconnect conn_u1
DROP ROLE r1, r2;
DROP USER u1, u2;

SET @@global.partial_revokes = @save_partial_revokes;

--echo Show what is logged:

--echo ------ rewrite ------
SELECT COUNT(*) = 8 AS NO_AS_CLAUSE_COUNT FROM mysql.general_log WHERE argument LIKE 'GRANT SELECT, INSERT ON *.* TO u2' AND command_type NOT LIKE 'Prepare';
SELECT COUNT(*) = 2 AS WITH_AS_WITHOUT_ROLE_COUNT FROM mysql.general_log WHERE argument LIKE 'GRANT SELECT, INSERT ON *.* TO u2 AS u1' AND command_type NOT LIKE 'Prepare';
SELECT COUNT(*) = 2 AS WITH_AS_WITH_ROLE_NONE_COUNT FROM mysql.general_log WHERE argument LIKE 'GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE NONE' AND command_type NOT LIKE 'Prepare';
SELECT COUNT(*) = 2 AS WITH_AS_WITH_ROLE_DEFAULT_COUNT FROM mysql.general_log WHERE argument LIKE 'GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE DEFAULT' AND command_type NOT LIKE 'Prepare';
SELECT COUNT(*) = 2 AS WITH_AS_WITH_ROLE_ALL_COUNT FROM mysql.general_log WHERE argument LIKE 'GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE ALL' AND command_type NOT LIKE 'Prepare';
SELECT COUNT(*) = 2 AS WITH_AS_WITH_ROLE_ALL_EXCEPT_COUNT FROM mysql.general_log WHERE argument LIKE 'GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE ALL EXCEPT r1' AND command_type NOT LIKE 'Prepare';
--echo ------ done ------

--remove_file $general_file_off

SET GLOBAL general_log_file= @old_general_log_file;
SET GLOBAL general_log= @old_general_log;
SET GLOBAL log_output= @old_log_output;
TRUNCATE TABLE mysql.general_log;

--echo -------------------------------------------------------------------------


# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--echo # Setup: clean environment
DROP USER IF EXISTS cu1@localhost;
DROP ROLE IF EXISTS r1, r2, `Role_Mix`, `space role`;

--echo # Create roles and user
CREATE ROLE r1, r2, `Role_Mix`, `space role`, r1@localhost;
CREATE USER cu1@localhost IDENTIFIED BY 'p';
GRANT r1, `Role_Mix`, `space role`, r1@localhost TO cu1@localhost;
GRANT ALL ON test.* TO cu1@localhost;

--echo # Connect as cu1
connect (con_cu1, localhost, cu1, p, test);
connection con_cu1;

--echo # Sanity: who am I
SELECT USER() AS login_user, CURRENT_USER() AS `current_user`;

--echo # CURRENT_USER_IN basic matches require full account user@host
SELECT CURRENT_USER_IN('cu1@localhost') AS should_be_1;
SELECT CURRENT_USER_IN('cu1@localhost, someone') AS should_be_1_from_list;
SELECT CURRENT_USER_IN('root@localhost') AS should_be_0_not_root;
--echo # Negative: user name without host must not match
SELECT CURRENT_USER_IN('cu1') AS missing_host_should_be_0;

--echo # Case sensitivity for user name (user names are case-sensitive)
SELECT CURRENT_USER_IN('CU1@localhost') AS should_be_0_case_sensitive_user;

--echo # Host handling: case-insensitive; account requires user@host
SELECT CURRENT_USER_IN('cu1@LOCALHOST') AS host_case_insensitive_should_be_1;
SELECT CURRENT_USER_IN('cu1@LoCaLhOsT') AS host_mixed_case_should_be_1;
SELECT CURRENT_USER_IN('cu1@127.0.0.1') AS host_specific_no_match_should_be_0;

--echo # Quoted/unquoted tokens inside the list should be accepted
SELECT CURRENT_USER_IN('"cu1@localhost"') AS double_quoted_token_should_be_0;
SELECT CURRENT_USER_IN('`cu1`@`LOCALHOST`') AS backticked_with_host_should_be_1;

--echo # Whitespace and CSV parsing
SELECT CURRENT_USER_IN('  cu1@localhost  ,    nobody  ') AS whitespace_csv_should_be_1;

--echo # NULL argument must return NULL
SELECT CURRENT_USER_IN(NULL) IS NULL AS null_returns_null;

--echo # Use in queries (WHERE) and const-for-execution behavior
CREATE TABLE tt(n INT);
INSERT INTO tt VALUES (1),(2),(3);
SELECT COUNT(*) AS cnt_all_rows FROM tt;
SELECT COUNT(*) AS cnt_when_user_matches FROM tt WHERE CURRENT_USER_IN('cu1@localhost');
SELECT COUNT(*) AS cnt_when_user_not_matching FROM tt WHERE CURRENT_USER_IN('root@localhost');
--echo # The result should be constant per execution
SELECT COUNT(DISTINCT CURRENT_USER_IN('cu1@localhost')) AS distinct_values_should_be_1 FROM tt;

--echo # CURRENT_ROLE_IN tests
--echo # Initially, no roles are active by default. Activate roles explicitly.
SET ROLE r1, `Role_Mix`, r1@localhost;
SELECT CURRENT_ROLE_IN('r1') AS r1_active_should_be_1;
SELECT CURRENT_ROLE_IN('r2') AS r2_not_active_should_be_0;
SELECT CURRENT_ROLE_IN('r2, r1') AS list_with_one_match_should_be_1;
SELECT CURRENT_ROLE_IN('"Role_Mix"') AS double_quoted_role_should_be_1;
SELECT CURRENT_ROLE_IN('`space role`') AS space_role_not_active_should_be_0;

--echo # Host handling for roles
SELECT CURRENT_ROLE_IN('r1@LOCALHOST') AS role_with_host_should_be_1;
SELECT CURRENT_ROLE_IN('r1@localhost') AS role_with_host_should_be_1;

--echo # Switch active roles
SET ROLE r1, `space role`;
SELECT CURRENT_ROLE_IN('`space role`') AS space_role_now_active_should_be_1;

--echo # Use in WHERE with roles and const-for-execution
SELECT COUNT(*) AS cnt_when_role_active FROM tt WHERE CURRENT_ROLE_IN('r1');
SET ROLE NONE;
SELECT CURRENT_ROLE_IN('r1') AS after_set_role_none_should_be_0;
SELECT COUNT(*) AS cnt_when_role_inactive FROM tt WHERE CURRENT_ROLE_IN('r1');
SELECT COUNT(DISTINCT CURRENT_ROLE_IN('r1')) AS distinct_values_should_be_1_or_0 FROM tt;

--echo # Back to root/default connection, verify CURRENT_USER_IN for root
connection default;
SELECT USER() AS login_user, CURRENT_USER() AS `current_user`;
SELECT CURRENT_USER_IN('root@localhost') AS root_should_be_1;
SELECT CURRENT_USER_IN('root@LOCALHOST') AS root_with_host_should_be_1;
--echo # Negative: user name without host must not match
SELECT CURRENT_USER_IN('root') AS root_missing_host_should_be_0;
SELECT CURRENT_USER_IN('ROOT@LOCALHOST') AS root_uppercase_should_be_0_case_sensitive_user;

--echo # Cleanup
connection default;
disconnect con_cu1;
DROP TABLE tt;
DROP USER cu1@localhost;
DROP ROLE r1, r2, `Role_Mix`, `space role`, r1@localhost;

# Wait until all sessions are disconnected
--source include/wait_until_count_sessions.inc

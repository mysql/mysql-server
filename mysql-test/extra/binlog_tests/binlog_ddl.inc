# ==== Purpose ====
# 2pc-aware DDL Query-log-event binlog event generator to be sourced from
# top-level files, e.g suite/binlog/t/binlog_crash_safe_ddl.test
# The file contains DDLs that are crash-safe
# which include at this point the ACL DDL:s,
# CREATE TRIGGER, DROP TRIGGER, CREATE FUNCTION, ALTER FUNCTION,
# DROP FUNCTION, CREATE PROCEDURE, DROP PROCEDURE, CREATE EVENT,
# ALTER EVENT, DROP EVENT, CREATE VIEW, ALTER VIEW, DROP VIEW.
# CREATE/DROP/ALTER TABLE, CREATE/DROP INDEX, TRUNCATE TABLE,
# RENAME TABLE
#
# ==== Implementation ====
# See comments in suite/binlog/t/binlog_crash_safe_ddl.test
#
# ==== References ====
# WL#9175  Correct recovery of DDL statements/transactions by binary log
#
# ==== Interfaces ====
# --let $binlog_file_create_user [ out ]  the variable value will be computed
#                                         and returned to the caller
#
#
# The tests logics corresponds to a loop of
# For $dll_query in create-user .. drop-user do { ... }
# Inside the loop evaluator $ddl_query goes through two crashes.
# It is re-issued after the 1st one where
# it is destined to roll back, which is proved by an assert.
# Unused 'pre*'crash_check must be specified explicitly with an empty assignment (var=).
#

--let $binlog_file=

--let $ddl_query=CREATE USER user1 IDENTIFIED WITH 'mysql_native_password'
--let $pre_binlog_crash_check=SELECT count(*) = 0 FROM mysql.user WHERE user = 'user1'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM mysql.user WHERE user = 'user1'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc
  # The rhs is computed by the above sourced macro
  --let $binlog_file_create_user= $binlog_file

--let $ddl_query=GRANT SELECT ON *.* TO user1
--let $pre_binlog_crash_check=SELECT select_priv = 'N' FROM mysql.user WHERE user = 'user1'
--let $post_binlog_crash_check=SELECT select_priv = 'Y' FROM mysql.user WHERE user = 'user1'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=REVOKE SELECT ON *.* FROM user1
--let $pre_binlog_crash_check=SELECT select_priv = 'Y' FROM mysql.user WHERE user = 'user1'
--let $post_binlog_crash_check=SELECT select_priv = 'N' FROM mysql.user WHERE user = 'user1'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $id_string='123456'
--let $old_auth_str=`SELECT authentication_string FROM mysql.user WHERE user LIKE 'user1'`
--let $ddl_query=ALTER USER user1 IDENTIFIED BY $id_string
--let $pre_binlog_crash_check=SELECT authentication_string = '$old_auth_str' FROM mysql.user WHERE user LIKE 'user1'
--let $post_binlog_crash_check=SELECT authentication_string != '$old_auth_str' FROM mysql.user WHERE user LIKE 'user1'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $id_string='abcdef'
--let $old_auth_str=`SELECT authentication_string FROM mysql.user WHERE user LIKE 'user1'`
--let $ddl_query=SET PASSWORD FOR user1=PASSWORD($id_string)
--let $pre_binlog_crash_check=SELECT authentication_string = '$old_auth_str' FROM mysql.user WHERE user LIKE 'user1'
--let $post_binlog_crash_check=SELECT authentication_string != '$old_auth_str' FROM mysql.user WHERE user LIKE 'user1'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=RENAME USER user1 TO user2
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM mysql.user WHERE user = 'user1'
--let $post_binlog_crash_check=SELECT count(*) = 0 FROM mysql.user WHERE user = 'user1'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

# post-recovery states in the empty crash_checks cases are verified by the following blocks
--let $ddl_query=CREATE ROLE roleA,roleB
# The pre-binlog-crash post-recovery state is effectively verified by
# the post-binlog crash simulation.
--let $pre_binlog_crash_check=SELECT COUNT(*) = 0 FROM mysql.user WHERE user LIKE 'roleB' OR user LIKE 'roleA';
--let $post_binlog_crash_check=SELECT COUNT(*) = 2 FROM mysql.user WHERE user LIKE 'roleB' OR user LIKE 'roleA';
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=GRANT roleA TO roleB
--let $pre_binlog_crash_check=SELECT count(*) = 0 FROM mysql.role_edges WHERE from_user = 'roleA'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM mysql.role_edges WHERE from_user = 'roleA'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=REVOKE roleA FROM roleB
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM mysql.role_edges WHERE from_user = 'roleA'
--let $post_binlog_crash_check=SELECT count(*) = 0 FROM mysql.role_edges WHERE from_user = 'roleA'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=ALTER USER user2 DEFAULT ROLE roleA;
--let $pre_binlog_crash_check=SELECT count(*) = 0 FROM mysql.default_roles WHERE user = 'user2'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM mysql.default_roles WHERE user = 'user2'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=DROP ROLE roleA,roleB
--let $role_num=`SELECT count(*) FROM mysql.user WHERE user LIKE 'roleA' OR user LIKE 'roleB'`
--let $pre_binlog_crash_check= SELECT count(*) = $role_num FROM mysql.user WHERE user LIKE 'roleA' OR user LIKE 'roleB'
--let $post_binlog_crash_check= SELECT count(*) = 0 FROM mysql.user WHERE user LIKE 'roleA' OR user LIKE 'roleB'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=DROP USER user2
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM mysql.user WHERE user = 'user2'
--let $post_binlog_crash_check=SELECT count(*) = 0 FROM mysql.user WHERE user = 'user2'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

#
# Prepared statement case
#
--let $user= user3
--eval PREPARE stmt FROM 'CREATE USER $user'
--let $ddl_query= EXECUTE stmt
--let $pre_binlog_crash_check=SELECT count(*) = 0 FROM mysql.user WHERE user = $user'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM mysql.user WHERE user = $user'
DEALLOCATE PREPARE stmt;

--eval PREPARE stmt FROM 'DROP USER $user'
--let $ddl_query= EXECUTE stmt
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM mysql.user WHERE user = $user'
--let $post_binlog_crash_check=SELECT count(*) = 0 FROM mysql.user WHERE user = $user'
DEALLOCATE PREPARE stmt;

#
# Extra:s
#
--let $ddl_query=CREATE TRIGGER tr_0 BEFORE INSERT ON $table FOR EACH ROW SET @a:=1
--let $pre_binlog_crash_check=SELECT count(*) = 0 FROM information_schema.TRIGGERS where TRIGGER_NAME = 'tr_0'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.TRIGGERS where TRIGGER_NAME = 'tr_0'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=DROP TRIGGER tr_0
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.TRIGGERS where TRIGGER_NAME = 'tr_0'
--let $post_binlog_crash_check=SELECT count(*) = 0 FROM information_schema.TRIGGERS where TRIGGER_NAME = 'tr_0'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

#
# WL9173: New data-dictionary: Improve crash-safety of non-table DDL.
#

--let $ddl_query=CREATE FUNCTION f_0() RETURNS INT RETURN 5;
--let $pre_binlog_crash_check=SELECT count(*) = 0 FROM information_schema.ROUTINES where ROUTINE_NAME = 'f_0'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.ROUTINES where ROUTINE_NAME = 'f_0'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=ALTER FUNCTION f_0 COMMENT 'comment'
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.ROUTINES where ROUTINE_NAME = 'f_0'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.ROUTINES where ROUTINE_NAME = 'f_0'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=DROP FUNCTION f_0
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.ROUTINES where ROUTINE_NAME = 'f_0'
--let $post_binlog_crash_check=SELECT count(*) = 0 FROM information_schema.ROUTINES where ROUTINE_NAME = 'f_0'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=CREATE PROCEDURE p_0() BEGIN END
--let $pre_binlog_crash_check=SELECT count(*) = 0 FROM information_schema.ROUTINES where ROUTINE_NAME = 'p_0'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.ROUTINES where ROUTINE_NAME = 'p_0'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=ALTER PROCEDURE p_0 COMMENT 'comment'
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.ROUTINES where ROUTINE_NAME = 'p_0'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.ROUTINES where ROUTINE_NAME = 'p_0'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=DROP PROCEDURE p_0
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.ROUTINES where ROUTINE_NAME = 'p_0'
--let $post_binlog_crash_check=SELECT count(*) = 0 FROM information_schema.ROUTINES where ROUTINE_NAME = 'p_0'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=CREATE EVENT e_0 ON SCHEDULE EVERY 2 DAY DO SELECT 2
--let $pre_binlog_crash_check=SELECT count(*) = 0 FROM information_schema.EVENTS where EVENT_NAME = 'e_0'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.EVENTS where EVENT_NAME = 'e_0'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=ALTER EVENT e_0 ON SCHEDULE EVERY 2 day DO SELECT 1;
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.EVENTS where EVENT_NAME = 'e_0'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.EVENTS where EVENT_NAME = 'e_0'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=DROP EVENT e_0
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.EVENTS where EVENT_NAME = 'e_0'
--let $post_binlog_crash_check=SELECT count(*) = 0 FROM information_schema.EVENTS where EVENT_NAME = 'e_0'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=CREATE VIEW v_0 AS SELECT * FROM $table
--let $pre_binlog_crash_check=SELECT count(*) = 0 FROM information_schema.VIEWS where TABLE_NAME = 'v_0'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.VIEWS where TABLE_NAME = 'v_0'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=ALTER VIEW v_0 AS SELECT * FROM $table
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.VIEWS where TABLE_NAME = 'v_0'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.VIEWS where TABLE_NAME = 'v_0'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query=DROP VIEW v_0
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.VIEWS where TABLE_NAME = 'v_0'
--let $post_binlog_crash_check=SELECT count(*) = 0 FROM information_schema.VIEWS where TABLE_NAME = 'v_0'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query= CREATE FUNCTION metaphon RETURNS STRING SONAME '$UDF_EXAMPLE_LIB'
--let $pre_binlog_crash_check=SELECT count(*) = 0 FROM mysql.func where NAME = 'metaphon'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM mysql.func where NAME = 'metaphon'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query= DROP FUNCTION metaphon;
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM mysql.func where NAME = 'metaphon'
--let $post_binlog_crash_check=SELECT count(*) = 0 FROM mysql.func where NAME = 'metaphon'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query= CREATE TABLE test.t1(a int)
--let $pre_binlog_crash_check=SELECT count(*)=0 FROM information_schema.tables where table_name='t1'
--let $post_binlog_crash_check=SELECT count(*)=1 FROM information_schema.tables where table_name='t1'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query= ALTER TABLE test.t1 ADD COLUMN t1_new_col varchar(100);
--let $pre_binlog_crash_check=SELECT count(*)=1 FROM information_schema.columns where table_name='t1'
--let $post_binlog_crash_check=SELECT count(*)=2 FROM information_schema.columns where table_name='t1'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query= ALTER TABLE test.t1 ADD index new_t1_idx(t1_new_col);
--let $pre_binlog_crash_check=SELECT count(*)=0 FROM information_schema.innodb_indexes where name='new_t1_idx'
--let $post_binlog_crash_check=SELECT count(*)=1 FROM information_schema.innodb_indexes where name='new_t1_idx'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query= ALTER TABLE test.t1 RENAME TO test.t2
--let $pre_binlog_crash_check=SELECT count(*)=0 FROM information_schema.tables where table_name='t2'
--let $post_binlog_crash_check=SELECT count(*)=1 FROM information_schema.tables where table_name='t2'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query= ALTER TABLE test.t2 DROP COLUMN a;
--let $pre_binlog_crash_check=SELECT count(*)=2 FROM information_schema.columns where table_name='t2'
--let $post_binlog_crash_check=SELECT count(*)=1 FROM information_schema.columns where table_name='t2'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query= ALTER TABLE test.t2 CHANGE COLUMN t1_new_col t2_new_col char(10);
--let $pre_binlog_crash_check=SELECT count(*)=0 FROM information_schema.columns where table_name='t2' and column_name='t2_new_col'
--let $post_binlog_crash_check=SELECT count(*)=1 FROM information_schema.columns where table_name='t2' and column_name='t2_new_col'

--let $ddl_query= ALTER TABLE test.t2 MODIFY COLUMN t2_new_col varchar(10);
--let $pre_binlog_crash_check=SELECT count(*)=0 FROM information_schema.columns where table_name='t2' and column_name='t2_new_col'
--let $post_binlog_crash_check=SELECT count(*)=1 FROM information_schema.columns where table_name='t2' and column_name='t2_new_col'

--let $ddl_query= TRUNCATE TABLE test.t2
--let $pre_binlog_crash_check=SELECT count(*)=1 FROM information_schema.tables where table_name='t2'
--let $post_binlog_crash_check=SELECT count(*)=1 FROM information_schema.tables where table_name='t2'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query= ALTER TABLE test.t2 ADD fulltext index full_t2_idx(t1_new_col);
--let $pre_binlog_crash_check=SELECT count(*)=0 FROM information_schema.innodb_indexes where name='full_t2_idx'
--let $post_binlog_crash_check=SELECT count(*)=1 FROM information_schema.innodb_indexes where name='full_t2_idx'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query= ALTER TABLE test.t2 DROP index full_t2_idx;
--let $pre_binlog_crash_check=SELECT count(*)=1 FROM information_schema.innodb_indexes where name='full_t2_idx'
--let $post_binlog_crash_check=SELECT count(*)=0 FROM information_schema.innodb_indexes where name='full_t2_idx'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query= DROP TABLE test.t2
--let $pre_binlog_crash_check=SELECT count(*)=1 FROM information_schema.tables where table_name='t2'
--let $post_binlog_crash_check=SELECT count(*)=0 FROM information_schema.tables where table_name='t2'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query= CREATE TABLESPACE ts ADD DATAFILE 'ts_0.ibd'
--let $pre_binlog_crash_check=SELECT count(*)=0 FROM information_schema.INNODB_TABLESPACES WHERE name = 'ts'
--let $post_binlog_crash_check=SELECT count(*)=1 FROM information_schema.INNODB_TABLESPACES WHERE name = 'ts'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

--let $ddl_query= DROP TABLESPACE ts
--let $pre_binlog_crash_check=SELECT count(*)=1 FROM information_schema.INNODB_TABLESPACES WHERE name = 'ts'
--let $post_binlog_crash_check=SELECT count(*)=0 FROM information_schema.INNODB_TABLESPACES WHERE name = 'ts'
  --source extra/binlog_tests/binlog_crash_safe_ddl.inc

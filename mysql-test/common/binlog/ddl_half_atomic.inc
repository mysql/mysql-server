# ==== Purpose ====
# 2pc-aware DDL Query-log-event binlog event generator to be sourced from
# top-level files, e.g suite/binlog/t/binlog_half_crash_safe_ddl.test
# The file contains DDLs that are not fully crash-safe
# which include at this point general DDL:s like CREATE|DROP|ALTER table.
# See common/binlog/ddl.inc for the fully atomic ddl list.
#
# TODO: Fully-atomic reading statements could be migrated into common/binlog/ddl.inc
#
# ==== Implementation ====
# See comments in suite/binlog/t/binlog_half_crash_safe_ddl.test

# The result file records content of the binary log which in particular demonstrates
# xid field is in there.
--let $do_show_binlog_events= 1

# For dll in create-, drop-, alter-, rename- table(s), ... do
# {

# CREATE TABLE, (not CREATE..SELECT)
--let $ddl_query=CREATE TABLE $table (a int auto_increment primary key)

--let $pre_binlog_crash_check=SELECT count(*) = 0 FROM information_schema.tables WHERE table_name = '$table'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.tables WHERE table_name = '$table'
  --source common/binlog/crash_safe_ddl.inc

# ALTER TABLE
--let $ddl_query_comment='Col b copy-added'
--let $ddl_query=ALTER TABLE $table ADD COLUMN b INT NOT NULL, ALGORITHM=COPY, COMMENT $ddl_query_comment
--let $post_binlog_crash_check=select TABLE_COMMENT LIKE $ddl_query_comment FROM information_schema.tables where table_name = '$table'
  --source common/binlog/crash_safe_ddl.inc

--let $ddl_query_comment='Index on b inplace-added'
--let $ddl_query=ALTER TABLE $table ADD INDEX idx(b), ALGORITHM= INPLACE, COMMENT $ddl_query_comment
--let $post_binlog_crash_check=select TABLE_COMMENT LIKE $ddl_query_comment FROM information_schema.tables where table_name = '$table'
  --source common/binlog/crash_safe_ddl.inc

--let $table_renamed=t_3
--let $ddl_query=ALTER TABLE $table RENAME $table_renamed
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.tables WHERE table_name = '$table_renamed'
  --source common/binlog/crash_safe_ddl.inc

# RENAME
--let $ddl_query=RENAME TABLE $table_renamed TO $table /* Name restored */
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.tables WHERE table_name = '$table_renamed'
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.tables WHERE table_name = '$table'
  --source common/binlog/crash_safe_ddl.inc

# TRUNCATE
# This one is apparently not a DDL, but rather a satelite to the next TRUNCATE
--let $ddl_query=INSERT INTO $table SET a=1,b=1
--let $post_binlog_crash_check=SELECT count(*) = 1 FROM $table
  --source common/binlog/crash_safe_ddl.inc
#
--let $ddl_query=TRUNCATE TABLE $table
#
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM $table
--let $post_binlog_crash_check=SELECT count(*) = 0 FROM $table
  --source common/binlog/crash_safe_ddl.inc

#
# TABLESPACE
#
--let $do_pre_binlog=0
--let $ts=ts_0
--let $ddl_query=CREATE TABLESPACE $ts ADD DATAFILE 'ts_0.ibd'
--let $pre_binlog_crash_check= SELECT COUNT(*) = 0 FROM information_schema.INNODB_TABLESPACES WHERE name = '$ts'
--let $post_binlog_crash_check=   SELECT COUNT(*) = 1 FROM information_schema.INNODB_TABLESPACES WHERE name = '$ts'
  --source common/binlog/crash_safe_ddl.inc

# The following feature is just not implemented expect for NDB, e.g:
#--error ER_CHECK_NOT_IMPLEMENTED
#--let $ddl_query=ALTER TABLESPACE $ts ADD DATAFILE 'ts_1.ibd'
# Otherwise the after crash checks would be:
#--let $pre_binlog_crash_check= SELECT COUNT(*) = 0 FROM information_schema.INNODB_TABLESPACES WHERE name = '$ts'
#--let $post_binlog_crash_check=   SELECT COUNT(*) = 1 FROM information_schema.INNODB_TABLESPACES WHERE name = '$ts'
#  --source common/binlog/crash_safe_ddl.inc

--let $ddl_query=DROP TABLESPACE $ts
--let $pre_binlog_crash_check= SELECT COUNT(*) = 1 FROM information_schema.INNODB_TABLESPACES WHERE name = '$ts'
--let $post_binlog_crash_check=   SELECT COUNT(*) = 0 FROM information_schema.INNODB_TABLESPACES WHERE name = '$ts'
  --source common/binlog/crash_safe_ddl.inc

#
# CREATE,ALTER,DROP DB
#

--let $db=db_0
--let $collate_old=latin1_general_ci
--let $collate_new=utf8mb3_general_ci

--let $ddl_query=CREATE DATABASE $db DEFAULT COLLATE $collate_old
--let $pre_binlog_crash_check= SELECT count(*) = 0 FROM information_schema.SCHEMATA WHERE schema_name = '$db'
--let $post_binlog_crash_check=   SELECT count(*) = 1 FROM information_schema.SCHEMATA WHERE schema_name = '$db'
  --source common/binlog/crash_safe_ddl.inc

--let $ddl_query=ALTER DATABASE $db DEFAULT COLLATE $collate_new
--let $pre_binlog_crash_check= SELECT count(*) = 1 FROM information_schema.SCHEMATA WHERE schema_name = '$db' and default_collation_name = '$collate_old'
--let $post_binlog_crash_check=   SELECT count(*) = 1 FROM information_schema.SCHEMATA WHERE schema_name = '$db' and default_collation_name = '$collate_new'
  --source common/binlog/crash_safe_ddl.inc

--let $ddl_query=DROP DATABASE $db
--let $pre_binlog_crash_check= SELECT count(*) = 1 FROM information_schema.SCHEMATA WHERE schema_name = '$db'
--let $post_binlog_crash_check=   SELECT count(*) = 0 FROM information_schema.SCHEMATA WHERE schema_name = '$db'
  --source common/binlog/crash_safe_ddl.inc

#
# The following DROP query is also a part of the test's total cleanup.
#

# DROP TABLE
--let $ddl_query=DROP TABLE $table
--let $pre_binlog_crash_check=SELECT count(*) = 1 FROM information_schema.tables WHERE table_name = '$table'
--let $post_binlog_crash_check=SELECT count(*) = 0 FROM information_schema.tables WHERE table_name = '$table'
  --source common/binlog/crash_safe_ddl.inc

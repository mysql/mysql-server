# ==== Purpose ====
#
# Verify that default_table_encryption is replicated when needed.
#
# ==== Requirements ====
#
# R1. @@session.default_table_encryption should be replicated for
#     CREATE TABLESPACE when no ENCRYPTION clause is included in the
#     statement.
#
# R2. @@session.default_table_encryption should be replicated for
#     CREATE DATABASE when no ENCRYPTION clause is included in the
#     statement.
#
# R3. @@session.default_table_encryption should not be replicated for
#     any other statements.
#
# ==== Implementation ====
#
# Try all possible combinations of values for
# @@global.default_table_encryption on master and slave, as well as
# @@session.default_table_encryption on master.
#
# For each such option combination, try three CREATE DATABASE and
# three CREATE TABLESPACE statements: without ENCRYPTION clause, with
# ENCRYPTION='Y', and with ENCRYPTION='N'.  Also try ALTER DATABASE
# and ALTER TABLESPACE.
#
# For each such option combination and statement, verify that the
# value of default_table_encryption is included or excluded in the
# binary log according to the requirements, and verify that the actual
# encryptedness of the database/tablespace is as expected on both
# master and slave.
#
# ==== References ====
#
# WL#12261: Control (enforce and disable) table encryption
# BUG#29818605: WRONG CONDITION TO REPLICATE AND WRONG PRIVILEGE TO SET DEFAULT_TABLE_ENCRYPTION

# Restart master and slave with keyring

--let $rpl_server_number= 1
--source include/rpl_stop_server.inc
--let $rpl_server_error= 0
--let $rpl_omit_print_server_parameters= 1
--source include/rpl_start_server.inc
--let $rpl_server_number= 2
--source include/rpl_stop_server.inc
--let $rpl_server_error= 0
--let $rpl_omit_print_server_parameters= 1
--source include/rpl_start_server.inc
--source include/rpl_connection_slave.inc
--source include/start_slave.inc

# Executes CREATE DATABASE or CREATE TABLESPACE,
# verifies that binlog contains default_table_encryption if expected,
# and verifies that it has the correct encryptedness on master and slave.
#
# Parameters:
#
#   $desc
#     Brief text explaining what is expected in the scenario
#
#   $statement
#     Statement to execute - CREATE DATABASE or CREATE TABLESPACE
#
#   $expect_binlog
#     If this is 0 or 1, assert that the binary log contains
#     default_table_encryption set to that value.  If set to empty
#     string, assert that the binary log does not contain
#     default_table_encryption.
#
#   $check_statement
#     Statement to check if encryption is enabled.
#
#   $expect
#     Assert that the result returned by $check_statement matches
#     (LIKE) $expect on both master and slave.
#
#   $drop_statement
#     Statement to "undo" the effect of $statement.  This will be
#     executed unless it is set to empty string.
#
--let $script_dir = $MYSQLTEST_VARDIR
--write_file $script_dir/check_encryption.inc
  --echo ==== $statement [master_session:$master_session master_global:$master_global slave_global:$slave_global] ====
  --echo # $desc
  # Restart slave threads to force them take the global value.
  --source include/rpl_connection_slave.inc
  --source include/stop_slave_sql.inc
  --source include/start_slave_sql.inc
  # Execute statement on master.
  --source include/rpl_connection_master.inc
  --source include/save_binlog_position.inc
  eval $statement;
  # Verify that the binary log contains the expected value
  --echo # Assert that binlog is as expected
  --let $mysqlbinlog_out = $MYSQLTEST_VARDIR/rpl_table_encryption_binlog.sql
  --exec $MYSQL_BINLOG $binlog_fullpath --start-position=$binlog_position > $mysqlbinlog_out
  --let $assert_file = $mysqlbinlog_out
  --let $assert_select = default_table_encryption
  if ($expect_binlog != '') {
    --let $assert_count =
    --let $assert_match = default_table_encryption=$expect_binlog
    --let $assert_text = Binlog should contain default_table_encryption=$expect_binlog
  }
  if ($expect_binlog == '') {
    --let $assert_count = 0
    --let $assert_match =
    --let $assert_text = Binlog should not contain default_table_encryption
  }
  --source include/assert_grep.inc
  --remove_file $mysqlbinlog_out
  # Verify that the definition is as expected on master.
  --echo # Assert that object on master is "$expect"
  --let $output = `$check_statement`
  --let $result = `SELECT "$output" LIKE "$expect"`
  # Verify that the definition is as expected on slave.
  if ($result) {
    --source include/sync_slave_sql_with_master.inc
    --echo # Assert that object on slave is "$expect"
    --let $output = `$check_statement`
    --let $result = `SELECT "$output" LIKE "$expect"`
  }
  # Print error
  if (!$result) {
    --let $connection = $CURRENT_CONNECTION
    --source include/show_rpl_debug_info.inc
    --echo master_global='$master_global'
    --echo slave_global='$slave_global'
    --echo master_session='$master_session'
    --echo statement='$statement'
    --echo check_statement='$check_statement'
    --echo output='$output'
    --echo expect='$expect'
    --echo result='$result'
    --echo connection='$connection'
    --die Error: the created object did not have the expected encryptedness on slave
  }
  # Clean up.
  --source include/rpl_connection_master.inc
  if ($drop_statement != '') {
    eval $drop_statement;
  }
  --source include/sync_slave_sql_with_master.inc
  --source include/rpl_connection_master.inc
EOF


--echo #### SETUP ####

--source include/rpl_connection_master.inc
SET @default_table_encryption_save = @@global.default_table_encryption;
--source include/sync_slave_sql_with_master.inc
SET @default_table_encryption_save = @@global.default_table_encryption;

# Loops to try all possible values of
# master:@@global.default_table_encryption,
# master:@@session.default_table_encryption, and
# slave:@@global.default_table_encryption.


--echo #### TEST ####

--let $master_global = 0
while ($master_global <= 1) {
  --source include/rpl_connection_master.inc
  eval SET @@global.default_table_encryption = $master_global;

  --let $slave_global = 0
  while ($slave_global <= 1) {
    --source include/rpl_connection_slave.inc
    eval SET @@global.default_table_encryption = $slave_global;

    --let $master_session = 0
    --let $letter = N
    while ($master_session <= 1) {
      --source include/rpl_connection_master.inc
      eval SET @@session.default_table_encryption = $master_session;

      --let $check_statement = SHOW CREATE DATABASE db

      --let $desc = CREATE without ENCRYPTION: include the variable in the binlog
      --let $statement = CREATE DATABASE db
      --let $drop_statement =
      --let $expect = %ENCRYPTION='$letter'%
      --let $expect_binlog = $master_session
      --source $script_dir/check_encryption.inc

      --let $desc = ALTER: exclude the variable from the binlog
      eval SET @@session.default_table_encryption = 1-$master_session;
      --let $statement = ALTER DATABASE db CHARACTER SET = utf8
      --let $drop_statement = DROP DATABASE db
      --let $expect = %ENCRYPTION='$letter'%
      --let $expect_binlog =
      --source $script_dir/check_encryption.inc
      eval SET @@session.default_table_encryption = $master_session;

      --let $desc = CREATE with ENCRYPTION: exclude the variable from the binlog
      --let $statement = CREATE DATABASE db DEFAULT ENCRYPTION = 'n'
      --let $drop_statement = DROP DATABASE db
      --let $expect = %ENCRYPTION='N'%
      --let $expect_binlog =
      --source $script_dir/check_encryption.inc

      --let $desc = CREATE with ENCRYPTION: exclude the variable from the binlog
      --let $statement = CREATE DATABASE db DEFAULT ENCRYPTION = 'y'
      --let $drop_statement = DROP DATABASE db
      --let $expect = %ENCRYPTION='Y'%
      --let $expect_binlog =
      --source $script_dir/check_encryption.inc

      --let $check_statement = SELECT ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME LIKE 'ts%'

      --let $desc = CREATE without ENCRYPTION: include the variable in the binlog
      --let $statement = CREATE TABLESPACE ts
      --let $drop_statement =
      --let $expect = $letter
      --let $expect_binlog = $master_session
      --source $script_dir/check_encryption.inc

      --let $desc = ALTER should not include the variable in the binlog
      eval SET @@session.default_table_encryption = 1-$master_session;
      --let $statement = ALTER TABLESPACE ts RENAME TO ts2
      --let $drop_statement = DROP TABLESPACE ts2
      --let $expect = $letter
      --let $expect_binlog =
      --source $script_dir/check_encryption.inc
      eval SET @@session.default_table_encryption = $master_session;

      --let $desc = CREATE with ENCRYPTION: exclude the variable from the binlog
      --let $statement = CREATE TABLESPACE ts ENCRYPTION = 'y'
      --let $drop_statement = DROP TABLESPACE ts
      --let $expect = Y
      --let $expect_binlog =
      --source $script_dir/check_encryption.inc

      --let $desc = CREATE with ENCRYPTION: exclude the variable from the binlog
      --let $statement = CREATE TABLESPACE ts ENCRYPTION = 'n'
      --let $drop_statement = DROP TABLESPACE ts
      --let $expect = N
      --let $expect_binlog =
      --source $script_dir/check_encryption.inc

      --inc $master_session
      --let $letter = Y
    }
    --inc $slave_global
  }
  --inc $master_global
}


--echo #### CLEANUP ####

--source include/rpl_connection_master.inc
SET @@global.default_table_encryption = @default_table_encryption_save;
--source include/rpl_connection_slave.inc
SET @@global.default_table_encryption = @default_table_encryption_save;

--remove_file $script_dir/check_encryption.inc

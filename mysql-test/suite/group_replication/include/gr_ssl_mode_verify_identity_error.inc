#
# Validate that Group Replication ssl mode works properly.
#
# Steps:
#   1. Start one member with GCS SSL enabled with ssl mode
#      VERIFY_IDENTITY.
#   2. Start the second member with GCS SSL enabled and
#      validating server certificate against the CA plus
#      validating that server hostname matches server
#      certificate hostname, the member will not be able to
#      join the group, since the certificate contains an invalid
#      hostname.
#   3. Clean up.
#
--source include/big_test.inc
--source include/check_openssl_version.inc
--source include/have_group_replication_plugin.inc
--let $rpl_skip_group_replication_start= 1
--source include/group_replication.inc

#check which stack are we using
--let $have_mysql_stack= `SELECT @@group_replication_communication_stack LIKE 'MySQL'`

--echo
--echo ############################################################
--echo # 1. Start one member with GCS SSL enabled.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
SET @group_replication_ssl_mode_save= @@GLOBAL.group_replication_ssl_mode;
SET GLOBAL group_replication_ssl_mode= VERIFY_IDENTITY;
--source include/start_and_bootstrap_group_replication.inc

--let $grep_file= $MYSQLTEST_VARDIR/tmp/group_replication_ssl_mode_verify_identity_error.1.err
--let $grep_pattern= Group communication SSL configuration: group_replication_ssl_mode: "VERIFY_IDENTITY"
--let $grep_output= print_count
--source include/grep_pattern.inc


--echo
--echo ############################################################
--echo # 2. Start the second member with GCS SSL enabled and
--echo #    validating server certificate against the CA plus
--echo #    validating that server hostname matches server
--echo #    certificate hostname, the member will not be able to
--echo #    join the group, since the certificate does contain the
--echo #    hostname 'localhost' and the hostname is '127.0.0.1'.
--let $rpl_connection_name= server2
--source include/rpl_connection.inc
SET @group_replication_ssl_mode_save= @@GLOBAL.group_replication_ssl_mode;

--disable_query_log
--eval SET GLOBAL group_replication_group_name= '$group_replication_group_name'
--enable_query_log
SET GLOBAL group_replication_ssl_mode= VERIFY_IDENTITY;

--error ER_GROUP_REPLICATION_CONFIGURATION
START GROUP_REPLICATION;

--let $assert_text= Check that Group Replication is running with SSL VERIFY_IDENTITY enabled
--let $assert_file= $MYSQLTEST_VARDIR/tmp/group_replication_ssl_mode_verify_identity_error.2.err
--let $assert_select= group_replication_ssl_mode: "VERIFY_IDENTITY"
--let $assert_count= 1
--source include/assert_grep.inc

if(!$have_mysql_stack) {
  --let $have_windows = `SELECT CONVERT(@@version_compile_os using latin1) IN ("Win32", "Win64", "Windows")`

  --let $assert_count_for_test=1
  if($have_windows) {
    --let $assert_count_for_test=2
  }

  --let $assert_text= Check that Group Replication errors out when a certificate with a wrong hostname is provided
  --let $assert_file= $MYSQLTEST_VARDIR/tmp/group_replication_ssl_mode_verify_identity_error.2.err
  --let $assert_only_after= XCom initialized and ready to accept incoming connections
  --let $assert_select= Failed to verify the server certificate via X509 certificate matching functions
  --let $assert_count= $assert_count_for_test
  --source include/assert_grep.inc
}

--echo
--echo ############################################################
--echo # 3. Clean up.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
SET GLOBAL group_replication_ssl_mode= @group_replication_ssl_mode_save;

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
SET GLOBAL group_replication_ssl_mode= @group_replication_ssl_mode_save;

--source include/group_replication_end.inc

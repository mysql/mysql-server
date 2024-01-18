#
# Validate that Group Replication SSL mode does not work with TLS v1.3 when
# an invalid configuration is set at server runtime.
#
# Steps:
# 0. Configure members to use encryption and save defaults.
# 1. 2 members group with OpenSSL 1.1.1
#    Verify that it is not possible to start group replication  when :
#      * TLS version is 1.3
#      * Intersection of the allowed TLS chipersuites for all the members is
#        empty.
#    server1: --tls-version='TLSv1.3'
#    server1: --tls-ciphersuites='TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384'
#    server2: --tls-version='TLSv1.3'
#    server2: --tls-ciphersuites='TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_CCM_SHA256'
#    Outcome: group will not work.
# 2. 2 members group with OpenSSL 1.1.1
#    Verify that it is not possible to join a node to a group when
#    TLS v1.3 is allowed but there is no compatible ciphersuite :
#      * TLS chipersuite of the node is incompatible  with the group, therefore
#        TLS v1.3 is not supported
#      * SLL cipher of the node is compatible with the group
#    --tls-version='TLSv1.3'
#    server1: --tls-ciphersuites='TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384'
#    server1: --ssl-cipher='ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384'
#    server2: --tls-ciphersuites='TLS_AES_128_CCM_SHA256'
#    server2: --ssl-cipher='DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384'
#    Outcome: group will not work.
# 3. Clean up.
#


--source include/big_test.inc
--source include/have_tlsv13.inc
--source include/have_group_replication_plugin.inc
--let $rpl_server_count= 2
--source include/force_restart.inc
--let $rpl_skip_group_replication_start= 1
--source include/group_replication.inc

#check which stack are we using
--let $have_mysql_stack= `SELECT @@group_replication_communication_stack LIKE 'MySQL'`

--echo
--echo ############################################################
--echo # 0. Configure members to use encryption and save defaults.
--let $rpl_connection_name= server1
--source include/connection.inc
SET @tls_version_save= @@GLOBAL.tls_version;
SET @tls_ciphersuites_save= @@GLOBAL.tls_ciphersuites;
SET @ssl_cipher_save= @@GLOBAL.ssl_cipher;
SET @group_replication_ssl_mode_save= @@GLOBAL.group_replication_ssl_mode;
SET @@GLOBAL.group_replication_ssl_mode= REQUIRED;

if($have_mysql_stack) {
  SET @tls_version_recovery_save= @@GLOBAL.group_replication_recovery_tls_version;
  SET @tls_ciphersuites_recovery_save= @@GLOBAL.group_replication_recovery_tls_ciphersuites;
  SET @ssl_cipher_recovery_save= @@GLOBAL.group_replication_recovery_ssl_cipher;
}

--let $rpl_connection_name= server2
--source include/connection.inc
SET @tls_version_save= @@GLOBAL.tls_version;
SET @tls_ciphersuites_save= @@GLOBAL.tls_ciphersuites;
SET @ssl_cipher_save= @@GLOBAL.ssl_cipher;
SET @group_replication_ssl_mode_save= @@GLOBAL.group_replication_ssl_mode;
SET @@GLOBAL.group_replication_ssl_mode= REQUIRED;

if($have_mysql_stack) {
  SET @tls_version_recovery_save= @@GLOBAL.group_replication_recovery_tls_version;
  SET @tls_ciphersuites_recovery_save= @@GLOBAL.group_replication_recovery_tls_ciphersuites;
  SET @ssl_cipher_recovery_save= @@GLOBAL.group_replication_recovery_ssl_cipher;
}

SET SESSION sql_log_bin= 0;
call mtr.add_suppression("\\[GCS\\] Error connecting to all peers. Member join failed. Local port:*");
call mtr.add_suppression("\\[GCS\\] The member was unable to join the group.*");
call mtr.add_suppression("Timeout on wait for view after joining group");
call mtr.add_suppression("Error connecting to the local group communication engine instance");
SET SESSION sql_log_bin= 1;

--echo
--echo ############################################################
--echo # 1. 2 members group with OpenSSL 1.1.1
--echo #    Verify that it is not possible to start group replication  when :
--echo #      * TLS version is 1.3
--echo #      * Intersection of the allowed TLS chipersuites for all the members
--echo #        is empty.
--echo #    server1: --tls-version='TLSv1.3'
--echo #    server1: --tls-ciphersuites='TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384'
--echo #    server2: --tls-version='TLSv1.3'
--echo #    server2: --tls-ciphersuites='TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_CCM_SHA256'
--echo #    Outcome: group will not work.

--let $rpl_connection_name= server1
--source include/connection.inc

SET @@GLOBAL.tls_version= 'TLSv1.3';
SET @@GLOBAL.tls_ciphersuites= 'TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384';
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= 'TLSv1.3';
  SET @@GLOBAL.group_replication_recovery_tls_ciphersuites= 'TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384';
}
ALTER INSTANCE RELOAD TLS;
--source include/start_and_bootstrap_group_replication.inc

# Try to add 2nd server
--let $rpl_connection_name= server2
--source include/connection.inc

--replace_result $group_replication_group_name GROUP_REPLICATION_GROUP_NAME
--eval SET GLOBAL group_replication_group_name= "$group_replication_group_name"

SET @@GLOBAL.tls_version= 'TLSv1.3';
SET @@GLOBAL.tls_ciphersuites= 'TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_CCM_SHA256';
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= 'TLSv1.3';
  SET @@GLOBAL.group_replication_recovery_tls_ciphersuites= 'TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_CCM_SHA256';
}
ALTER INSTANCE RELOAD TLS;

--error ER_GROUP_REPLICATION_CONFIGURATION
START GROUP_REPLICATION;

if(!$have_mysql_stack) {
  --let $assert_file = $MYSQLTEST_VARDIR/tmp/gr_ssl_tls13_runtime_invalid_configuration.2.err
  --let $assert_only_after=  .*XCom initialized and ready to accept incoming connections.*
  --let $assert_select =.*Error connecting using SSL.*
  --let $assert_match =.*Error connecting using SSL.*
  --let $assert_text = Found the expected error line in the server log.
  --source include/assert_grep.inc
}

--let $rpl_connection_name= server1
--source include/connection.inc
--source include/stop_group_replication.inc


--echo
--echo ############################################################
--echo # 2. 2 members group with OpenSSL 1.1.1
--echo #    Verify that it is not possible to join a node to a group when
--echo #    TLS v1.3 is allowed but there is no compatible ciphersuite :
--echo #      * TLS chipersuite of the node is incompatible  with the group,
--echo #        therefore TLS v1.3 is not supported
--echo #      * SLL cipher of the node is compatible with the group
--echo #    --tls-version='TLSv1.3'
--echo #    server1: --tls-ciphersuites='TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384'
--echo #    server1: --ssl-cipher='ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384'
--echo #    server2: --tls-ciphersuites='TLS_AES_128_CCM_SHA256'
--echo #    server2: --ssl-cipher='DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384'
--echo #    Outcome: group will not work.

--let $rpl_connection_name= server1
--source include/connection.inc

SET @@GLOBAL.tls_version= 'TLSv1.3';
SET @@GLOBAL.ssl_cipher= 'ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384';
SET @@GLOBAL.tls_ciphersuites= 'TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384';
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= 'TLSv1.3';
  SET @@GLOBAL.group_replication_recovery_ssl_cipher= 'ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384';
  SET @@GLOBAL.group_replication_recovery_tls_ciphersuites= 'TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384';
}
ALTER INSTANCE RELOAD TLS;
--source include/start_and_bootstrap_group_replication.inc

# Try to add 2nd server
--let $rpl_connection_name= server2
--source include/connection.inc

--replace_result $group_replication_group_name GROUP_REPLICATION_GROUP_NAME
--eval SET GLOBAL group_replication_group_name= "$group_replication_group_name"

SET @@GLOBAL.tls_version= 'TLSv1.3';
SET @@GLOBAL.ssl_cipher= 'DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384';
SET @@GLOBAL.tls_ciphersuites = 'TLS_AES_128_CCM_SHA256';
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= 'TLSv1.3';
  SET @@GLOBAL.group_replication_recovery_ssl_cipher= 'DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384';
  SET @@GLOBAL.group_replication_recovery_tls_ciphersuites= 'TLS_AES_128_CCM_SHA256';
}
ALTER INSTANCE RELOAD TLS;

--error ER_GROUP_REPLICATION_CONFIGURATION
START GROUP_REPLICATION;

if(!$have_mysql_stack) {
  --let $assert_file = $MYSQLTEST_VARDIR/tmp/gr_ssl_tls13_runtime_invalid_configuration.2.err
  --let $assert_only_after=  .*XCom initialized and ready to accept incoming connections.*
  --let $assert_select =.*Error connecting using SSL.*
  --let $assert_match =.*Error connecting using SSL.*
  --let $assert_text = Found the expected error line in the server log.
  --source include/assert_grep.inc
}

--let $rpl_connection_name= server1
--source include/connection.inc
--source include/stop_group_replication.inc


--echo
--echo ############################################################
--echo # 3. Clean up.

--let $rpl_connection_name= server1
--source include/connection.inc
SET @@GLOBAL.group_replication_ssl_mode= @group_replication_ssl_mode_save;
SET @@GLOBAL.tls_version= @tls_version_save;
SET @@GLOBAL.tls_ciphersuites= @tls_ciphersuites_save;
SET @@GLOBAL.ssl_cipher = @ssl_cipher_save;
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= @tls_version_recovery_save;
  SET @@GLOBAL.group_replication_recovery_tls_ciphersuites= @tls_ciphersuites_recovery_save;
  SET @@GLOBAL.group_replication_recovery_ssl_cipher= @ssl_cipher_recovery_save;
}
ALTER INSTANCE RELOAD TLS;

--let $rpl_connection_name= server2
--source include/connection.inc
SET @@GLOBAL.group_replication_ssl_mode= @group_replication_ssl_mode_save;
SET @@GLOBAL.tls_version= @tls_version_save;
SET @@GLOBAL.tls_ciphersuites= @tls_ciphersuites_save;
SET @@GLOBAL.ssl_cipher = @ssl_cipher_save;
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= @tls_version_recovery_save;
  SET @@GLOBAL.group_replication_recovery_tls_ciphersuites= @tls_ciphersuites_recovery_save;
  SET @@GLOBAL.group_replication_recovery_ssl_cipher= @ssl_cipher_recovery_save;
}
ALTER INSTANCE RELOAD TLS;


--source include/group_replication_end.inc

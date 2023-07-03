#
# Validate that Group Replication ssl mode works properly with TLSv1.3.
#
# Steps:
# 0. Configure members to use encryption and save defaults.
# 1. 2 members group with OpenSSL 1.1.1
#    No --tls-ciphersuites
#    No --tls-version
#    Outcome: group will work.
# 2. 2 members group with OpenSSL 1.1.1
#    No --tls-ciphersuites
#    server1: --tls-version='TLSv1.2,TLSv1.3'
#    server2: --tls-version='TLSv1.2'
#    Outcome: group will work.
# 3. 2 members group with OpenSSL 1.1.1
#    No --tls-ciphersuites
#    server1: --tls-version='TLSv1.3'
#    server2: --tls-version='TLSv1.2'
#    Outcome: group will not work.
# 4. 2 members group with OpenSSL 1.1.1
#    No --tls-ciphersuites
#    server1: --tls-version='TLSv1.3'
#    server2: --tls-version='TLSv1.3'
#    Outcome: group will work.
# 5. 2 members group with OpenSSL 1.1.1
#    --tls-ciphersuites='TLS_AES_256_GCM_SHA384'
#    server1: --tls-version='TLSv1.2,TLSv1.3'
#    server2: --tls-version='TLSv1.2,TLSv1.3'
#    Outcome: group will work.
# 6. 2 members group with OpenSSL 1.1.1
#    --tls-ciphersuites= '', which will disable all ciphers.
#    No --tls-version
#    Outcome: group will not work.
# 7. Clean up.
#
--source include/big_test.inc
--source include/have_tlsv13.inc
--source include/have_group_replication_plugin.inc
--source include/force_restart.inc
--let $rpl_skip_group_replication_start= 1
--source include/group_replication.inc


#check which stack are we using
--let $have_mysql_stack= `SELECT @@group_replication_communication_stack LIKE 'MySQL'`

--echo
--echo ############################################################
--echo # 0. Configure members to use encryption and save defaults.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
SET @tls_version_save= @@GLOBAL.tls_version;
SET @tls_ciphersuites_save= @@GLOBAL.tls_ciphersuites;
SET @group_replication_ssl_mode_save= @@GLOBAL.group_replication_ssl_mode;

--let $tls_version_recovery_save_s1=
--let $tls_ciphersuites_recovery_save_s1=
if($have_mysql_stack) {
  --let $tls_version_recovery_save_s1= @@GLOBAL.group_replication_recovery_tls_version;
  --let $tls_ciphersuites_recovery_save_s1= @@GLOBAL.group_replication_recovery_tls_ciphersuites;
}

SET @@GLOBAL.group_replication_ssl_mode= REQUIRED;

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
SET @tls_version_save= @@GLOBAL.tls_version;
SET @tls_ciphersuites_save= @@GLOBAL.tls_ciphersuites;
SET @group_replication_ssl_mode_save= @@GLOBAL.group_replication_ssl_mode;
SET @@GLOBAL.group_replication_ssl_mode= REQUIRED;

--let $tls_version_recovery_save_s2=
--let $tls_ciphersuites_recovery_save_s2=
if($have_mysql_stack) {
  --let $tls_version_recovery_save_s2= @@GLOBAL.group_replication_recovery_tls_version;
  --let $tls_ciphersuites_recovery_save_s2= @@GLOBAL.group_replication_recovery_tls_ciphersuites;
}

SET SESSION sql_log_bin= 0;
call mtr.add_suppression("\\[GCS\\] Error connecting to all peers. Member join failed. Local port:*");
call mtr.add_suppression("\\[GCS\\] The member was unable to join the group.*");
call mtr.add_suppression("Timeout on wait for view after joining group");
SET SESSION sql_log_bin= 1;


--echo
--echo ############################################################
--echo # 1. 2 members group with OpenSSL 1.1.1
--echo #    No --tls-ciphersuites
--echo #    No --tls-version
--echo #    Outcome: group will work.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
--source include/start_and_bootstrap_group_replication.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
--source include/start_group_replication.inc

--let $group_replication_number_of_members= 2
--source include/gr_wait_for_number_of_members.inc

--let $rpl_connection_name= server1
--source include/rpl_connection.inc
--source include/stop_group_replication.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
--source include/stop_group_replication.inc


--echo
--echo ############################################################
--echo # 2. 2 members group with OpenSSL 1.1.1
--echo #    No --tls-ciphersuites
--echo #    server1: --tls-version='TLSv1.2,TLSv1.3'
--echo #    server2: --tls-version='TLSv1.2'
--echo #    Outcome: group will work.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc

SET @@GLOBAL.tls_version= 'TLSv1.2,TLSv1.3';
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= 'TLSv1.2,TLSv1.3';
}
ALTER INSTANCE RELOAD TLS;
--source include/start_and_bootstrap_group_replication.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc

SET @@GLOBAL.tls_version= 'TLSv1.2';
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= 'TLSv1.2';
}
ALTER INSTANCE RELOAD TLS;
--source include/start_group_replication.inc

--let $group_replication_number_of_members= 2
--source include/gr_wait_for_number_of_members.inc

--let $rpl_connection_name= server1
--source include/rpl_connection.inc
--source include/stop_group_replication.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
--source include/stop_group_replication.inc


--echo
--echo ############################################################
--echo # 3. 2 members group with OpenSSL 1.1.1
--echo #    No --tls-ciphersuites
--echo #    server1: --tls-version='TLSv1.3'
--echo #    server2: --tls-version='TLSv1.2'
--echo #    Outcome: group will not work.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc

SET @@GLOBAL.tls_version= 'TLSv1.3';
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= 'TLSv1.3';
}
ALTER INSTANCE RELOAD TLS;
--source include/start_and_bootstrap_group_replication.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc

SET @@GLOBAL.tls_version= 'TLSv1.2';
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= 'TLSv1.2';
}
ALTER INSTANCE RELOAD TLS;

--error ER_GROUP_REPLICATION_CONFIGURATION
START GROUP_REPLICATION;

--let $rpl_connection_name= server1
--source include/rpl_connection.inc
--source include/stop_group_replication.inc


--echo
--echo ############################################################
--echo # 4. 2 members group with OpenSSL 1.1.1
--echo #    No --tls-ciphersuites
--echo #    server1: --tls-version='TLSv1.3'
--echo #    server2: --tls-version='TLSv1.3'
--echo #    Outcome: group will work.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc

SET @@GLOBAL.tls_version= 'TLSv1.3';
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= 'TLSv1.3';
}
ALTER INSTANCE RELOAD TLS;
--source include/start_and_bootstrap_group_replication.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc

SET @@GLOBAL.tls_version= 'TLSv1.3';
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= 'TLSv1.3';
}
ALTER INSTANCE RELOAD TLS;
--source include/start_group_replication.inc

--let $group_replication_number_of_members= 2
--source include/gr_wait_for_number_of_members.inc

--let $rpl_connection_name= server1
--source include/rpl_connection.inc
--source include/stop_group_replication.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
--source include/stop_group_replication.inc


--echo
--echo ############################################################
--echo # 5. 2 members group with OpenSSL 1.1.1
--echo #    --tls-ciphersuites='TLS_AES_256_GCM_SHA384'
--echo #    server1: --tls-version='TLSv1.2,TLSv1.3'
--echo #    server2: --tls-version='TLSv1.2,TLSv1.3'
--echo #    Outcome: group will work.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc

SET @@GLOBAL.tls_version= 'TLSv1.2,TLSv1.3';
SET @@GLOBAL.tls_ciphersuites= 'TLS_AES_256_GCM_SHA384';
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= 'TLSv1.2,TLSv1.3';
  SET @@GLOBAL.group_replication_recovery_tls_ciphersuites= 'TLS_AES_256_GCM_SHA384';
}
ALTER INSTANCE RELOAD TLS;
--source include/start_and_bootstrap_group_replication.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc

SET @@GLOBAL.tls_version= 'TLSv1.2,TLSv1.3';
SET @@GLOBAL.tls_ciphersuites= 'TLS_AES_256_GCM_SHA384';
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= 'TLSv1.2,TLSv1.3';
  SET @@GLOBAL.group_replication_recovery_tls_ciphersuites= 'TLS_AES_256_GCM_SHA384';
}
ALTER INSTANCE RELOAD TLS;
--source include/start_group_replication.inc

--let $group_replication_number_of_members= 2
--source include/gr_wait_for_number_of_members.inc

--let $rpl_connection_name= server1
--source include/rpl_connection.inc
--source include/stop_group_replication.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
--source include/stop_group_replication.inc


--echo
--echo ############################################################
--echo # 6. 2 members group with OpenSSL 1.1.1
--echo #    --tls-ciphersuites= '', which will disable all ciphers.
--echo #    No --tls-version
--echo #    Outcome: group will not work.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
--source include/gr_set_bootstrap_group.inc

SET @@GLOBAL.tls_ciphersuites= '';
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_ciphersuites= '';
}
ALTER INSTANCE RELOAD TLS;

--error ER_GROUP_REPLICATION_CONFIGURATION
START GROUP_REPLICATION;

--source include/gr_clear_bootstrap_group.inc

if(!$have_mysql_stack) {
  --let $grep_file= $MYSQLTEST_VARDIR/tmp/gr_ssl_tls13.1.err
  --let $grep_pattern= Error connecting using SSL
  --let $grep_output= boolean
  --source include/grep_pattern.inc
}

--echo
--echo ############################################################
--echo # 7. Clean up.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
SET @@GLOBAL.group_replication_ssl_mode= @group_replication_ssl_mode_save;
SET @@GLOBAL.tls_version= @tls_version_save;
SET @@GLOBAL.tls_ciphersuites= @tls_ciphersuites_save;
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= $tls_version_recovery_save_s1;
  SET @@GLOBAL.group_replication_recovery_tls_ciphersuites= $tls_ciphersuites_recovery_save_s1;
}
ALTER INSTANCE RELOAD TLS;

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
SET @@GLOBAL.group_replication_ssl_mode= @group_replication_ssl_mode_save;
SET @@GLOBAL.tls_version= @tls_version_save;
SET @@GLOBAL.tls_ciphersuites= @tls_ciphersuites_save;
if($have_mysql_stack) {
  SET @@GLOBAL.group_replication_recovery_tls_version= $tls_version_recovery_save_s2;
  SET @@GLOBAL.group_replication_recovery_tls_ciphersuites= $tls_ciphersuites_recovery_save_s2;
}
ALTER INSTANCE RELOAD TLS;

--source include/group_replication_end.inc

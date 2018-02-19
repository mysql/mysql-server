include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
call mtr.add_suppression("Master command COM_REGISTER_SLAVE failed.");
call mtr.add_suppression(" * does not exist in the InnoDB internal");
include/assert.inc [On master, the table should return an empty set.]

# Verify that SELECT works for every field and produces an output
# similar to the corresponding field in SHOW SLAVE STATUS(SSS).

include/assert.inc [Value returned by SSS and PS table for Host should be same.]
include/assert.inc [Value returned by SSS and PS table for Port should be same.]
include/assert.inc [Value returned by SSS and PS table for User should be same.]
include/assert.inc [Value returned by SSS and PS table for Network_Interface should be same.]
include/assert.inc [Value returned by SSS and PS table for Auto_Position should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_Allowed should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_CA_File should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_CA_Path should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_Certificate should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_Cipher should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_Key should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_Verify_Server_Certificate should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_Crl_File should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_Crl_Path should be same.]
include/assert.inc [Value returned by SSS and PS table for Connection_Retry_Interval should be same.]
include/assert.inc [Value returned by SSS and PS table for Connection_Retry_Count should be same.]

# Change configuration parameters and verify that these changes
# are shown correctly by SELECTs from PS table.

create user replssl@localhost require ssl;
grant replication slave on *.* to replssl@localhost;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
change master to
master_user= 'replssl',
master_password= '',
master_retry_count= 1,
master_ssl= 1,
master_ssl_ca= 'MYSQL_TEST_DIR/std_data/cacert.pem',
master_ssl_cert= 'MYSQL_TEST_DIR/std_data/client-cert.pem',
master_ssl_key= 'MYSQL_TEST_DIR/std_data/client-key.pem';
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.

# Checking SSL parameters, they were empty in tests done in the
# previous section.

include/assert.inc [Value returned by SSS and PS table for SSL_Allowed should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_CA_File should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_CA_Path should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_Certificate should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_Cipher should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_Key should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_Verify_Server_Certificate should be same.]
change master to master_ssl=0;
change master to master_ssl_crl= 'MYSQL_TEST_DIR/std_data/crl-client-revoked.crl', master_ssl_crlpath= 'MYSQL_TEST_DIR/std_data/crldir';
include/assert.inc [Value returned by SSS and PS table for SSL_Crl_File should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_Crl_Path should be same.]
include/assert.inc [Value returned by SSS and PS table for TLS version should be same.]

# Test with servers in gtid-mode=on.

include/rpl_restart_server.inc [server_number=1 gtids=on]
include/rpl_restart_server.inc [server_number=2 gtids=on]

# 1) Test for Auto_position= 0.

include/assert.inc [Value returned by SSS and PS table for Auto_Position should be same.]
change master to
master_user = 'root',
master_auto_position= 1;
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.

# 2) Test for Auto_position= 1.

include/assert.inc [Value returned by SSS and PS table for Auto_Position should be same.]
include/rpl_end.inc

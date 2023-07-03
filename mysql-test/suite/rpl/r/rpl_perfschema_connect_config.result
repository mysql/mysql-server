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
CHANGE REPLICATION SOURCE to
SOURCE_USER= 'replssl',
SOURCE_PASSWORD= '',
SOURCE_RETRY_COUNT= 1,
SOURCE_SSL= 1,
SOURCE_SSL_CA= 'MYSQL_TEST_DIR/std_data/cacert.pem',
SOURCE_SSL_CERT= 'MYSQL_TEST_DIR/std_data/client-cert.pem',
SOURCE_SSL_KEY= 'MYSQL_TEST_DIR/std_data/client-key.pem';
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
CHANGE REPLICATION SOURCE to SOURCE_SSL=0;
CHANGE REPLICATION SOURCE to SOURCE_SSL_CRL= 'MYSQL_TEST_DIR/std_data/crl-client-revoked.crl', SOURCE_SSL_CRLPATH= 'MYSQL_TEST_DIR/std_data/crldir';
include/assert.inc [Value returned by SSS and PS table for SSL_Crl_File should be same.]
include/assert.inc [Value returned by SSS and PS table for SSL_Crl_Path should be same.]
include/assert.inc [Value returned by SSS and PS table for TLS version should be same.]

# Test with servers in gtid-mode=on.

include/rpl_restart_server.inc [server_number=1 gtids=on]
include/rpl_restart_server.inc [server_number=2 gtids=on]

# 1) Test for Auto_position= 0.

include/assert.inc [Value returned by SSS and PS table for Auto_Position should be same.]
CHANGE REPLICATION SOURCE to
SOURCE_USER = 'root',
SOURCE_AUTO_POSITION= 1;
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.

# 2) Test for Auto_position= 1.

include/assert.inc [Value returned by SSS and PS table for Auto_Position should be same.]
CHANGE REPLICATION SOURCE to
SOURCE_AUTO_POSITION= 0,
SOURCE_SSL_CRL= '',
SOURCE_SSL_CRLPATH= '',
SOURCE_SSL_CA= '',
SOURCE_SSL_CERT= '',
SOURCE_SSL_KEY='';
include/rpl_set_gtid_mode.inc
drop user replssl@localhost;
drop user replssl@localhost;
include/rpl_end.inc

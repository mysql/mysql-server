## XPLUGIN: Following test cases for mysqlx plugin SSL connection
#           and status variables testing.
# Modified : 19-08-2015 Lalit Choudhary

--echo Preamble
--source ../include/have_performance_schema_threads.inc
--source ../include/xplugin_preamble.inc

SET GLOBAL mysqlx_connect_timeout = 300;
call mtr.add_suppression("Unsuccessful login attempt");

## Test starts here
--write_file $MYSQL_TMP_DIR/mysqlx-enable-ssl.tmp
Mysqlx.Connection.CapabilitiesGet {
}

-->recvtype Mysqlx.Connection.Capabilities

Mysqlx.Connection.CapabilitiesSet {
  capabilities {
    capabilities {
      name: "tls"
      value {
	type: SCALAR
	scalar {
	  type: V_BOOL
	  v_bool: 1
	}
      }
    }
  }
}
-->recvtype Mysqlx.Ok
-->echo Enable SSL
-->enablessl

Mysqlx.Connection.CapabilitiesGet {
}

-->recvtype Mysqlx.Connection.Capabilities
EOF
--exec $MYSQLXTEST -u root --password='' -h127.0.0.1 --no-auth --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem --file=$MYSQL_TMP_DIR/mysqlx-enable-ssl.tmp 2>&1

--write_file $MYSQL_TMP_DIR/mysqlx-setcapabilities-tls-invalid.tmp
-->echo setting read/write  tls param with possible invalid data types V_SINT,V_UINT ,V_NULL,V_OCTETS ,V_DOUBLE,V_FLOAT,V_STRING types instead of Bool
Mysqlx.Connection.CapabilitiesGet {
}

-->recvtype Mysqlx.Connection.Capabilities

Mysqlx.Connection.CapabilitiesSet {
  capabilities {
    capabilities {
      name: "tls"
      value {
        type: SCALAR
        scalar {
          type: V_BOOL
          v_bool: 0
        }
      }
    }
  }
}

-->recv

Mysqlx.Connection.CapabilitiesSet {
  capabilities {
    capabilities {
      name: "tls"
      value {
        type: SCALAR
        scalar {
          type: V_NULL
        }
      }
    }
 }
}
-->recv

Mysqlx.Connection.CapabilitiesSet {
  capabilities {
    capabilities {
      name: "tls"
      value {
        type: SCALAR
        scalar {
          type:V_OCTETS
        }
      }
    }
  }
}
-->recv

Mysqlx.Connection.CapabilitiesSet {
  capabilities {
    capabilities {
      name: "tls"
      value {
        type: SCALAR
        scalar {
          type:V_UINT
        }
      }
    }
  }
}
-->recv

Mysqlx.Connection.CapabilitiesSet {
  capabilities {
    capabilities {
      name: "tls"
      value {
        type: SCALAR
        scalar {
          type:V_SINT
        }
      }
    }
  }
}
-->recv

###commenting as below throws inconsistency results
#Mysqlx.Connection.CapabilitiesSet {
#  capabilities {
#    capabilities {
#      name: "tls"
#      value {
#        type: SCALAR
#        scalar {
#          type: V_FLOAT
#          v_float:19.49
#        }
#      }
#    }
#  }
#}
#-->recv

#Mysqlx.Connection.CapabilitiesSet {
#  capabilities {
#    capabilities {
#      name: "tls"
#      value {
#        type: SCALAR
#        scalar {
#          type: V_DOUBLE
#          v_double:1111111
#        }
#      }
#    }
#  }
#}
#-->recv

EOF

##setting tls caps with ssl connection
--exec $MYSQLXTEST -u root --password='' -h127.0.0.1 --no-auth --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem --file=$MYSQL_TMP_DIR/mysqlx-setcapabilities-tls-invalid.tmp 2>&1

##setting tls caps with non-ssl connection
--exec $MYSQLXTEST -u root --password='' -h127.0.0.1 --no-auth --file=$MYSQL_TMP_DIR/mysqlx-setcapabilities-tls-invalid.tmp 2>&1

--echo Cleanup

#SET GLOBAL mysqlx_connect_timeout = 1;
SET GLOBAL mysqlx_connect_timeout = 300;

--remove_file $MYSQL_TMP_DIR/mysqlx-enable-ssl.tmp

--echo
# Tests added by QA
--echo
--echo ## SSL connection testing
##

## Test starts here
--write_file $MYSQL_TMP_DIR/mysqlx-connection-setup.tmp
-->sql

CREATE USER user1_mysqlx@localhost
            IDENTIFIED WITH 'mysql_native_password' BY 'auth_string1';
CREATE USER user2_mysqlx@localhost
            IDENTIFIED WITH 'mysql_native_password' BY 'auth_string2'
            PASSWORD EXPIRE;

# User connection with sha256_password plugin is not supported yet.
#CREATE USER user3_mysqlx@localhost
#             IDENTIFIED WITH 'sha256_password' BY 'auth_string3';

CREATE USER user3_mysqlx@localhost
IDENTIFIED WITH 'mysql_native_password' BY 'auth_string3'
REQUIRE CIPHER "DHE-RSA-AES256-SHA" AND
SUBJECT "/C=SE/ST=Stockholm/L=Stockholm/O=Oracle/OU=MySQL/CN=Client"
ISSUER "/C=SE/ST=Stockholm/L=Stockholm/O=Oracle/OU=MySQL/CN=CA"
PASSWORD EXPIRE NEVER;

CREATE USER user4_mysqlx@localhost REQUIRE SSL ACCOUNT LOCK;

CREATE USER user5_mysqlx@localhost
            IDENTIFIED WITH 'mysql_native_password' BY 'auth_string'
            REQUIRE SSL;

CREATE USER user6_mysqlx@localhost
            IDENTIFIED WITH 'mysql_native_password' BY 'dwh@#ghd'
            REQUIRE x509;

# openssl ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:SRP-DSS-AES-256-CBC-SHA:SRP-RSA-AES-256-CBC-SHA:SRP-AES-256-CBC-SHA:DHE-DSS-AES256-GCM-SHA384:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA256:DHE-RSA-AES256-SHA:DHE-DSS-AES256-SHA:DHE-RSA-CAMELLIA256-SHA:DHE-DSS-CAMELLIA256-SHA:ECDH-RSA-AES256-GCM-SHA384:ECDH-ECDSA-AES256-GCM-SHA384:ECDH-RSA-AES256-SHA384:ECDH-ECDSA-AES256-SHA384:ECDH-RSA-AES256-SHA:ECDH-ECDSA-AES256-SHA:AES256-GCM-SHA384:AES256-SHA256:AES256-SHA:CAMELLIA256-SHA:PSK-AES256-CBC-SHA:ECDHE-RSA-DES-CBC3-SHA:ECDHE-ECDSA-DES-CBC3-SHA:SRP-DSS-3DES-EDE-CBC-SHA:SRP-RSA-3DES-EDE-CBC-SHA:SRP-3DES-EDE-CBC-SHA:EDH-RSA-DES-CBC3-SHA:EDH-DSS-DES-CBC3-SHA:ECDH-RSA-DES-CBC3-SHA:ECDH-ECDSA-DES-CBC3-SHA:DES-CBC3-SHA:PSK-3DES-EDE-CBC-SHA:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA
# yassl   DHE-RSA-AES256-SHA:AES256-SHA:DHE-RSA-AES128-SHA:AES128-SHA:AES256-RMD:AES128-RMD:DES-CBC3-RMD:DHE-RSA-AES256-RMD:DHE-RSA-AES128-RMD:DHE-RSA-DES-CBC3-RMD:RC4-SHA:RC4-MD5:DES-CBC3-SHA:DES-CBC-SHA:EDH-RSA-DES-CBC3-SHA:EDH-RSA-DES-CBC-SHA
CREATE USER user7_mysqlx@localhost
            IDENTIFIED WITH 'mysql_native_password' BY ''
            REQUIRE CIPHER "AES256-SHA";

GRANT ALL ON *.* TO user1_mysqlx@localhost;
GRANT ALL ON *.* TO user2_mysqlx@localhost;
GRANT ALL ON *.* TO user3_mysqlx@localhost;
GRANT ALL ON *.* TO user4_mysqlx@localhost;
GRANT ALL ON *.* TO user5_mysqlx@localhost;
GRANT ALL ON *.* TO user6_mysqlx@localhost;
GRANT ALL ON *.* TO user7_mysqlx@localhost;
-->endsql
EOF

--exec $MYSQLXTEST -u root --password='' --file=$MYSQL_TMP_DIR/mysqlx-connection-setup.tmp 2>&1

--write_file $MYSQL_TMP_DIR/mysqlx-connection-info.tmp
-->sql
SELECT CONNECTION_TYPE from performance_schema.threads where processlist_command='Query';
SELECT USER();
# Bug #21690095: Mysqlx_ssl_version status variable showing different version on PB2.
# Tetscase has to be enabled when the bug is fixed.
SHOW STATUS LIKE 'Mysqlx_ssl_version';
SHOW STATUS LIKE 'Mysqlx_ssl_accepts';
SHOW STATUS LIKE 'Mysqlx_ssl_finished_accepts';
SHOW STATUS LIKE 'Mysqlx_ssl_cipher';
-->endsql
EOF

# Bug#21619518 -- FIXED
--exec $MYSQLXTEST -u user1_mysqlx --password='auth_string1' --tls-version=TLSv1 --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp 2>&1

# Try connecting with Expired password using user2_mysqlx user.
--let $expected_error_msg= Your password has expired. To log in you must change it using a client that supports expired passwords. \(code 1045\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST -u user2_mysqlx --password='auth_string2' --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem --tls-version=TLSv1 --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp 2>&1

# Using "connect-expired-password" Option to allow expired password.

--write_file $MYSQL_TMP_DIR/mysqlx-alter-pwd.tmp
-->sql
ALTER USER USER() IDENTIFIED BY 'alter-new-auth';
-->endsql
EOF

--write_file $MYSQL_TMP_DIR/mysqlx-set-pwd.tmp
-->sql
SET PASSWORD='set-new-auth';
-->endsql
EOF


--write_file $MYSQL_TMP_DIR/mysqlx-status.tmp
-->sql
SHOW STATUS WHERE `Variable_name` RLIKE '^Mysqlx_ssl_(cipher_list|ctx_verify_(depth|mode)|server_not_(after|before)|verify_(depth|mode))$';
-->endsql
EOF


# RESET PASSWORD FOR MYSQLXTEST CONNECT-EXPIRED-PASSWORD WITH SSL OPTIONS.
--exec $MYSQLXTEST -u user2_mysqlx --password='auth_string2' --connect-expired-password --ssl-cipher='DHE-RSA-AES256-SHA' --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem --file=$MYSQL_TMP_DIR/mysqlx-alter-pwd.tmp 2>&1
ALTER USER user2_mysqlx@localhost PASSWORD EXPIRE;
--exec $MYSQLXTEST -u user2_mysqlx --password='alter-new-auth' --connect-expired-password --ssl-cipher='DHE-RSA-AES256-SHA' --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem --file=$MYSQL_TMP_DIR/mysqlx-set-pwd.tmp 2>&1

# Testing connection with --connect-expired-password and without SSL options.
--exec $MYSQLXTEST -u user2_mysqlx --password='set-new-auth' --connect-expired-password --file=$MYSQL_TMP_DIR/mysqlx-alter-pwd.tmp 2>&1

ALTER USER user2_mysqlx@localhost PASSWORD EXPIRE;

--exec $MYSQLXTEST -u user2_mysqlx --password='alter-new-auth' --connect-expired-password --file=$MYSQL_TMP_DIR/mysqlx-set-pwd.tmp 2>&1

--exec $MYSQLXTEST -u user2_mysqlx --password='set-new-auth' --connect-expired-password --ssl-cipher='DHE-RSA-AES256-SHA' --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem --tls-version=TLSv1 --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp 2>&1

--exec $MYSQLXTEST -u user3_mysqlx --ssl-cipher='DHE-RSA-AES256-SHA' --password='auth_string3' --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp --tls-version=TLSv1 --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem  2>&1

--echo Testing conenction for ACCOUNT LOCK user.
--let $expected_error_msg=  Account is locked. \(code 1045\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST -u user4_mysqlx --password='' --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp --tls-version=TLSv1 --ssl-cipher='DHE-RSA-AES256-SHA' --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem 2>&1

--echo Unlocking user account.
ALTER USER user4_mysqlx@localhost ACCOUNT UNLOCK;
--exec $MYSQLXTEST -u user4_mysqlx --password='' --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cipher='DHE-RSA-AES256-SHA' --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem --tls-version=TLSv1 --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp 2>&1

--exec $MYSQLXTEST -u user5_mysqlx --password='auth_string' --ssl-cipher='DHE-RSA-AES256-SHA' --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem --tls-version=TLSv1 --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp 2>&1

--exec $MYSQLXTEST -u user5_mysqlx --password='auth_string' --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --tls-version=TLSv1 --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp 2>&1

--exec $MYSQLXTEST -u user6_mysqlx --password='dwh@#ghd' --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --ssl-cipher='DHE-RSA-AES256-SHA' --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem --tls-version=TLSv1 --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp 2>&1

--let $expected_error_msg=  Current account requires TLS to be activate. \(code 1045\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST -u user6_mysqlx --password='dwh@#ghd' --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --tls-version=TLSv1 --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp 2>&1

--let $expected_error_msg=  Current account requires TLS to be activate. \(code 1045\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST -u user7_mysqlx --tls-version=TLSv1 --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp 2>&1

--let $expected_error_msg= Current user cipher isn't allowed. \(code 1045\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST -u user7_mysqlx --ssl-cipher="DHE-RSA-AES256-SHA" --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem --tls-version=TLSv1 --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp 2>&1

--exec $MYSQLXTEST -u user7_mysqlx --ssl-cipher="AES256-SHA"         --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem --tls-version=TLSv1 --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp 2>&1

--echo Cert needed
--let $expected_error_msg= Current account requires TLS to be activate. \(code 1045\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST -u user7_mysqlx --ssl-cipher="AES256-SHA"         --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --tls-version=TLSv1 --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp 2>&1

--replace_regex /4294967295/-1/ /18446744073709551615/-1/
SHOW GLOBAL STATUS WHERE `Variable_name` RLIKE '^Mysqlx_ssl_(cipher_list|ctx_verify_(depth|mode)|server_not_(after|before)|verify_(depth|mode))$';

--replace_regex /4294967295/-1/ /18446744073709551615/-1/
--exec $MYSQLXTEST -u root --ssl-cipher="AES256-SHA"         --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem  --file=$MYSQL_TMP_DIR/mysqlx-status.tmp 2>&1


# Lets confirm that Mysqlx plugin can't use TCP-IP connections without SSL as secure one
call mtr.add_suppression("Plugin mysqlx reported: '.+: Unsuccessful login attempt: Secure transport required. To log in you must use TCP.SSL or UNIX socket connection.");
SET GLOBAL require_secure_transport:=1;
--let $expected_error_msg= Secure transport required. To log in you must use TCP\+SSL or UNIX socket connection. \(code 1045\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST -uroot --password='' --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp 2>&1
--exec $MYSQLXTEST -uroot --password='' --file=$MYSQL_TMP_DIR/mysqlx-connection-info.tmp --tls-version=TLSv1 --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem 2>&1
SET GLOBAL require_secure_transport:=0;


# Postamble
UNINSTALL PLUGIN mysqlx;

# Checking existence of mysqlx plugin status variables after uninstalling it.
SHOW STATUS LIKE 'mysqlx%';


# Cleanup
--remove_file $MYSQL_TMP_DIR/mysqlx-connection-setup.tmp
--remove_file $MYSQL_TMP_DIR/mysqlx-connection-info.tmp
--remove_file $MYSQL_TMP_DIR/mysqlx-alter-pwd.tmp
--remove_file $MYSQL_TMP_DIR/mysqlx-set-pwd.tmp
--remove_file $MYSQL_TMP_DIR/mysqlx-setcapabilities-tls-invalid.tmp
--remove_file $MYSQL_TMP_DIR/mysqlx-status.tmp

DROP USER user1_mysqlx@localhost, user2_mysqlx@localhost, user3_mysqlx@localhost,
          user4_mysqlx@localhost, user5_mysqlx@localhost, user6_mysqlx@localhost,
          user7_mysqlx@localhost;



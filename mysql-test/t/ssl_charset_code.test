################################################################################
# In step 1 the test checks if the client can properly connect with SSL.       #
# In step 2 the SSL character set code sent in handshake response by the client#
# is modified, so the server gets mismatched value. In that case server should #
# reject the client.                                                           #
#                                                                              #
# Bug #29916390: FR: REMOVE ASSERTION FAILED: CHARSET_CODE == SSL_CHARSET_CODE #
################################################################################

# the below is used to skip the test if the CLIENT is debug build
--source include/mysql_have_debug.inc


# prepare -create user to connect
CREATE USER 'ssl_charset_code_user'@'%' REQUIRE SSL;
GRANT ALL ON *.* TO 'ssl_charset_code_user'@'%' ;

# step 1
echo "Step 1 connect correctly.";
exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --user=ssl_charset_code_user --ssl-mode=REQUIRED -e "SET @ssl_charset_code_var=1" mysql;

# step 2
echo "Step 2 connect with mismatched character set code.";
error 1;
exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --user=ssl_charset_code_user --debug="d,simulate_bad_ssl_charset_code"
	--ssl-mode=REQUIRED -e "SET @ssl_charset_code_var=1" mysql;

# Cleanup
DROP USER 'ssl_charset_code_user'@'%';
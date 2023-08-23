CREATE USER u1@localhost IDENTIFIED BY 'secret' REQUIRE SSL;
GRANT SELECT ON test.* TO u1@localhost;

--replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
--exec $MYSQL -uu1 -psecret -h127.0.0.1 --ssl-ca=$MYSQL_TEST_DIR/std_data/ca-sha512.pem --ssl-cipher=ECDHE-RSA-AES128-GCM-SHA256 test -e "SHOW VARIABLES like 'ssl_cipher';"

DROP USER u1@localhost;


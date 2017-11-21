--source include/big_test.inc

--echo #
--echo # BUG# 26974113 - CONTRIBUTION BY FACEBOOK; READ COMPRESSED PACKETS OF SIZE 0XFFFFFF
--echo #

# mysqltest needs to be invoked with --compress option. Write to a file and then invoke mysql test with compress option.
--write_file $MYSQL_TMP_DIR/bug26974113.test
SET GLOBAL max_allowed_packet=33554432;
connect(con1, localhost, root,,);
connection con1;
SELECT REPEAT('T', 16777211);
disconnect con1;
connection default;
SET GLOBAL MAX_ALLOWED_PACKET=default;
EOF

--echo # The below SQL commands shall be run with --compress option.
--cat_file $MYSQL_TMP_DIR/bug26974113.test
# Run mysqltest with --compress option the above sql file.
--exec $MYSQL_TEST --compress < $MYSQL_TMP_DIR/bug26974113.test 1>$MYSQL_TMP_DIR/bug26974113_result.out 2> $MYSQL_TMP_DIR/bug26974113_err.out

# There should be no errors on the above run.
--cat_file $MYSQL_TMP_DIR/bug26974113_err.out

--remove_file $MYSQL_TMP_DIR/bug26974113.test
--remove_file $MYSQL_TMP_DIR/bug26974113_result.out
--remove_file $MYSQL_TMP_DIR/bug26974113_err.out

# End of test
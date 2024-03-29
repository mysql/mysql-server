# Create users with default digest-rounds
CREATE USER 'change_pwd_alter'@'%' IDENTIFIED WITH caching_sha2_password BY 'alter1';
CREATE USER 'change_pwd_set'@'%' IDENTIFIED WITH caching_sha2_password BY 'set1';


--let $TEST_ID=1
--let $MYSQLXTEST_INC=$MYSQLXTEST --ssl-mode=required

if ($test_restart_with_different_digest)
{
  --echo
  --echo #
  --echo # $test_group.$TEST_ID
  --let $new_digest_rounds=`SELECT @@GLOBAL.caching_sha2_password_digest_rounds * 2`

  --let $restart_parameters = restart: --caching_sha2_password_digest_rounds=$new_digest_rounds
  --source include/restart_mysqld.inc

  # Create user with changed digest-rounds (if $test_restart_with_different_digest was set)
  CREATE USER 'create_pwd'@'%' IDENTIFIED WITH caching_sha2_password BY 'create2';
  --inc $TEST_ID
}


--echo
--echo #
--echo # $test_group.$TEST_ID
--inc $TEST_ID
exec $MYSQLXTEST_INC
  -uchange_pwd_alter
  --password='alter1'
   --sql "ALTER USER 'change_pwd_alter'@'%' IDENTIFIED BY 'alter2'" 2>&1;

exec $MYSQLXTEST_INC
  -uchange_pwd_set
  --password='set1'
   --sql "SET PASSWORD='set2'" 2>&1;


--echo
--echo #
--echo # $test_group.$TEST_ID
--inc $TEST_ID
exec $MYSQLXTEST_INC
  -uchange_pwd_alter
  --password='alter1'
  --expect-error 1045
   --sql "SELECT THIS_MUST_BE_NOT_EXECUTED()" 2>&1;

exec $MYSQLXTEST_INC
  -uchange_pwd_set
  --password='set1'
  --expect-error 1045
   --sql "SELECT THIS_MUST_BE_NOT_EXECUTED()" 2>&1;


--echo
--echo #
--echo # $test_group.$TEST_ID
--inc $TEST_ID
exec $MYSQLXTEST_INC
  -uchange_pwd_alter
  --password='alter2'
   --sql "SELECT 'ok' as THIS_MUST_BE_EXECUTED" 2>&1;

exec $MYSQLXTEST_INC
  -uchange_pwd_set
  --password='set2'
   --sql "SELECT 'ok' as THIS_MUST_BE_EXECUTED" 2>&1;


if ($test_restart_with_different_digest)
{
  exec $MYSQLXTEST_INC
    -ucreate_pwd
    --password='create2'
     --sql "SELECT 'ok' as THIS_MUST_BE_EXECUTED" 2>&1;
  # Restore server to default configuration
  --let $restart_parameters = restart:
  --source include/restart_mysqld.inc
}

DROP USER 'change_pwd_alter'@'%';
DROP USER 'change_pwd_set'@'%';
DROP USER IF EXISTS 'create_pwd'@'%';

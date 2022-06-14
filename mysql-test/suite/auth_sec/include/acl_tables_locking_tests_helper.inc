#
# Usage:
# --let $SQL=<SQL statement>
# --let $EXPECT_ERROR = 0 | 1
# --let $EXPECTED_ERROR_NUMBER = <error_number>
# --source acl_tables_locking_tests_helper.inc
#

--enable_connect_log

CREATE USER u1 IDENTIFIED BY 'abcd';
GRANT SELECT, UPDATE ON *.* TO u1;

--connect(conn_u1, localhost, u1, abcd,,,)
SET @@autocommit=0;
--connect(conn_root_1, localhost, root,,,,)
SET @@lock_wait_timeout=1; 

--connection conn_u1
--eval $SQL

--connection conn_root_1
if ($EXPECT_ERROR) {
  --error $EXPECTED_ERROR_NUMBER
  CREATE USER u2;
  --error $EXPECTED_ERROR_NUMBER
  CREATE ROLE r1;
  --error $EXPECTED_ERROR_NUMBER
  GRANT SELECT ON *.* TO r1;
  --error $EXPECTED_ERROR_NUMBER
  GRANT r1 TO u2;
  --error $EXPECTED_ERROR_NUMBER
  SET DEFAULT ROLE r1 TO u2;
  --error $EXPECTED_ERROR_NUMBER
  ALTER USER u2 IDENTIFIED BY 'abcd';
  --error $EXPECTED_ERROR_NUMBER
  RENAME USER u2 TO u3;
  --error $EXPECTED_ERROR_NUMBER
  SET PASSWORD FOR u2 = 'abcd';
  --error $EXPECTED_ERROR_NUMBER
  REVOKE r1 FROM u2;
  --error $EXPECTED_ERROR_NUMBER
  REVOKE SELECT ON *.* FROM r1;
  --error $EXPECTED_ERROR_NUMBER
  DROP ROLE r1;
  --error $EXPECTED_ERROR_NUMBER
  DROP USER u2;
}

if (!$EXPECT_ERROR)
{
  CREATE USER u2;
  CREATE ROLE r1;
  GRANT SELECT ON *.* TO r1;
  GRANT r1 TO u2;
  SET DEFAULT ROLE r1 TO u2;
  ALTER USER u2 IDENTIFIED BY 'abcd';
  RENAME USER u2 TO u3;
  RENAME USER u3 TO u2;
  SET PASSWORD FOR u2 = 'abcd';
  REVOKE r1 FROM u2;
  REVOKE SELECT ON *.* FROM r1;
  DROP ROLE r1;
  DROP USER u2;
}

--connection conn_u1
ROLLBACK;

--connection conn_root_1
CREATE USER u2;
CREATE ROLE r1;
GRANT SELECT ON *.* TO r1;
GRANT r1 TO u2;
SET DEFAULT ROLE r1 TO u2;
ALTER USER u2 IDENTIFIED BY 'abcd';
RENAME USER u2 TO u3;
RENAME USER u3 TO u2;
SET PASSWORD FOR u2 = 'abcd';
REVOKE r1 FROM u2;
REVOKE SELECT ON *.* FROM r1;
DROP ROLE r1;
DROP USER u2;

# Testcase cleanup
--connection default
--disconnect conn_root_1
--disconnect conn_u1
DROP USER u1;

--disable_connect_log

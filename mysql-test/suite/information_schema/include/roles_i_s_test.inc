
--echo # Connect as $USER_NAME
connect(con1, localhost, $USER_NAME, foo, test);

--echo # With the roles activated during connection.
eval SELECT * FROM INFORMATION_SCHEMA.$TEST_TABLE $ORDER_BY;
SELECT CURRENT_ROLE();
SHOW GRANTS;

--echo # Enable roles;
eval SET ROLE $ROLE_LIST;
eval SELECT * FROM INFORMATION_SCHEMA.$TEST_TABLE $ORDER_BY;
SELECT CURRENT_ROLE();
SHOW GRANTS;

--echo Disconnect $USER_NAME
disconnect con1;
connection default;

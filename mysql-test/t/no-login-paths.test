--echo # WL#12429: Add a --no-login-paths option to turn off login paths from being processed

--echo # Creating a mysql user
CREATE USER myuser;

--echo # Creating a login file with login path "client" and credential "user=myuser"
--exec $MYSQL_CONFIG_EDITOR set --user=myuser 2>&1
--exec $MYSQL_CONFIG_EDITOR print --all

--echo # Connecting without --no-login-paths option, login file should be read and myuser should be used
--exec $MYSQL -e "SELECT CURRENT_USER();" 2>&1

--echo # Connecting with --no-login-paths option, login file should not be read and defaults-file configurations should be used (i.e. user=root)
--exec $MYSQL --no-login-paths -e "SELECT CURRENT_USER();" 2>&1

--echo # Using --login-path and --no-login-paths options together, error should be raised
--error 7
--exec $MYSQL --no-login-paths --login-path=client 2>&1

--error 2
--exec $MYSQL --login-path=client --no-login-paths 2>&1

--echo # Using --no-login-paths option after other client option (e.g. --user), error should be raised
--error 2
--exec $MYSQL --user=root --no-login-paths 2>&1

--echo # Specifying --no-login-paths before --no-defaults, error should be raised
--error 2
--exec $EXE_MYSQL --no-login-paths --no-defaults --port=13150 -e "SELECT current_user();" 2>&1

# Cleanup
DROP USER myuser;
--remove_file $MYSQL_TMP_DIR/.mylogin.cnf

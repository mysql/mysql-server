# This test case is a simplified version of
# plugin_auth_caching_sha2_password_multi_format_support.inc test.
# It duplicates the tests that are done for secondary passwords in
# plugin_auth_caching_sha2_password_multi_format_support.inc.
# The test was re-written to use standard accounts with a single password.
# X Protocol doesn't support dual passwords, thus this file was created for it.

# Short summary:
# 1. Check if CREATE user uses selected storage format.
# 2. Check if enforce storage format is set, then accounts with other storage
#    format are forced to change the password (expired).
# 3. Verify that fast auth doesn't work for accounts that have wrong storage
#    format.
#
# Please note that those points apply only to caching_sha2_password plugin.

#
# $parameter_client_script -
#    Set the application that will execute scripts in mysql-test language
#
# $parameter_client -
#    Set the application that will execute single queries
#
#
## Usage
#
# --let $parameter_client_script = $MYSQL_TEST --connect-expired-password
# --let $parameter_client = $MYSQL --connect-expired-password
# --source suite/auth_sec/include/plugin_auth_caching_sha2_password_multi_format_no_dual.inc

if (`SELECT '$parameter_client_script' = ''`)
{
  --die "plugin_auth_caching_sha2_password_multi_format_support.inc" requires "parameter_client_script" parameter.
}

if (`SELECT '$parameter_client' = ''`)
{
  --die "plugin_auth_caching_sha2_password_multi_format_support.inc" requires "parameter_client" parameter.
}

# Save initial setting
SELECT @@global.caching_sha2_password_storage_format INTO @saved_storage_format;
SELECT @@global.caching_sha2_password_enforce_storage_format INTO @saved_enforce_storage_format;

SET GLOBAL caching_sha2_password_storage_format="CRYPT5";

# -------------------------------------------------------------------
# Prepare script files
#
# Please note that the language of mysqlxtest and mysqltest is similar but not
# the same. To ensure compatibility, apply the following rules for those
# scripts:
#
# * Don't use the "--" version of the commands.
# * Variable substitution for queries for both tools is only done under "eval"
#   command.
# * Use "error" command to specify expected error returned by the server.
# * Use env var to pass parameters from this file to the script.

--let $query_account_file= $MYSQL_TMP_DIR/query_account_file.test
--write_file $query_account_file
SELECT CURRENT_USER();
EOF

--let $query_simple_file= $MYSQL_TMP_DIR/query_simple_file.test
--write_file $query_simple_file
SELECT 1;
EOF


--let $query_account_and_change_pwd_file= $MYSQL_TMP_DIR/query_account_and_change_pwd_file.test
--write_file $query_account_and_change_pwd_file
SELECT CURRENT_USER();

eval SET PASSWORD = '$TEST_PARAM_PASS';

EOF

--let $query_simple_er_expired_file= $MYSQL_TMP_DIR/query_simple_er_expired_file.test
--write_file $query_simple_er_expired_file
error ER_MUST_CHANGE_PASSWORD;
SELECT 1;
EOF

--let $query_simple_er_expired_change_pwd_file= $MYSQL_TMP_DIR/query_simple_er_expired_change_pwd_file.test
--write_file $query_simple_er_expired_change_pwd_file
error ER_MUST_CHANGE_PASSWORD;
SELECT 1;

eval SET PASSWORD = '$TEST_PARAM_PASS';

SELECT 1;
EOF

# -------------------------------------------------------------------
# The test begins here

SET GLOBAL caching_sha2_password_storage_format="PBKDF2_SHA512";
CREATE USER arthur IDENTIFIED WITH caching_sha2_password BY 'abcd';
CREATE USER marvin IDENTIFIED WITH caching_sha2_password BY 'efgh';

SET GLOBAL caching_sha2_password_storage_format="CRYPT5";
CREATE USER zaphod IDENTIFIED WITH caching_sha2_password BY 'ijkl';
CREATE USER ford IDENTIFIED WITH caching_sha2_password BY 'mnop';


# Current state
# User    Algo              Pass
#-------------------------------
# arthur  PBKDF2_SHA512     abcd
# marvin  PBKDF2_SHA512     efgh
# zaphod  CRYPT5			ijkl
# ford    CRYPT5			mnop

SELECT user, SUBSTRING(authentication_string, 1, 7) as first, SUBSTRING(JSON_EXTRACT(User_attributes, "$.additional_password"), 2, 7) as second FROM mysql.user WHERE user IN ('arthur', 'marvin', 'zaphod', 'ford') ORDER BY user ASC;

# Try with password
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uarthur -pabcd -e "SELECT CURRENT_USER()" 2>&1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -umarvin -pefgh -e "SELECT CURRENT_USER()" 2>&1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uzaphod -pijkl -e "SELECT CURRENT_USER()" 2>&1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uford -pmnop -e "SELECT CURRENT_USER()" 2>&1


FLUSH PRIVILEGES;

# Tests for caching_sha2_password_enforce_format

# Current state
# User    Algo              Pass
#-------------------------------
# arthur  PBKDF2_SHA512     abcd
# marvin  PBKDF2_SHA512     efgh
# zaphod  CRYPT5			ijkl
# ford    CRYPT5			mnop

SET GLOBAL caching_sha2_password_storage_format="PBKDF2_SHA512";
SET GLOBAL caching_sha2_password_enforce_storage_format=1;

# Try connecting with password stored in non-compliant format
exec $parameter_client_script
  --host=localhost --ssl-mode=REQUIRED
  -uzaphod -pijkl
  -x $query_simple_er_expired_file 2>&1;

# Try using fast auth - Must fail
--error 1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uzaphod -pijkl -e "SELECT CURRENT_USER()" 2>&1

# Try connecing with another password stored in non-compliant format
exec $parameter_client_script
  --host=localhost --ssl-mode=REQUIRED
  -uford -pmnop
  -x $query_simple_er_expired_file 2>&1;

# Try using fast auth - Must fail
--error 1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uford -pmnop -e "SELECT CURRENT_USER()" 2>&1

# Try connecting with password stored in non-compliant format and reset password
--let TEST_PARAM_PASS=ijkl
exec $parameter_client_script
  --host=localhost --ssl-mode=REQUIRED
  -uzaphod -pijkl
  -x $query_simple_er_expired_change_pwd_file 2>&1;

# Try using fast auth - Must fail
--error 1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uzaphod -pijkl -e "SELECT CURRENT_USER()" 2>&1

# Must succeed - secure channel
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uzaphod -pijkl -e "SELECT CURRENT_USER()" 2>&1

# Must succeed with fast auth now - compliant password format
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uzaphod -pijkl -e "SELECT CURRENT_USER()" 2>&1

# Try connecing with another password stored in non-compliant format and reset password
--let TEST_PARAM_PASS=mnop
exec $parameter_client_script
  --host=localhost --ssl-mode=REQUIRED
  -uford -pmnop
  -x $query_simple_er_expired_change_pwd_file 2>&1;

# Try using fast auth - Must fail
--error 1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uford -pmnop -e "SELECT CURRENT_USER()" 2>&1

# Must succeed - secure channel
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uford -pmnop -e "SELECT CURRENT_USER()" 2>&1

# Must succeed with fast auth now - compliant password format
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uford -pmnop -e "SELECT CURRENT_USER()" 2>&1

SET GLOBAL caching_sha2_password_storage_format="CRYPT5";
SET GLOBAL caching_sha2_password_enforce_storage_format=1;

# Current state
# User    Algo              Pass
#-------------------------------
# arthur  PBKDF2_SHA512     abcd
# marvin  PBKDF2_SHA512     efgh
# zaphod  CRYPT5			ijkl
# ford    CRYPT5			mnop

  # Try connecting with password stored in non-compliant format
exec $parameter_client_script
  --host=localhost --ssl-mode=REQUIRED
  -uarthur -pabcd
  -x $query_simple_er_expired_file 2>&1;

# Try using fast auth - Must fail
--error 1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uarthur -pabcd -e "SELECT CURRENT_USER()" 2>&1



# Try connecing with another password stored in non-compliant format
exec $parameter_client_script
  --host=localhost --ssl-mode=REQUIRED
  -umarvin -pefgh
  -x $query_simple_er_expired_file 2>&1;

# Try using fast auth - Must fail
--error 1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -umarvin -pefgh -e "SELECT CURRENT_USER()" 2>&1

# Try connecting with password stored in non-compliant format and reset password
let TEST_PARAM_PASS=abcd;
exec $parameter_client_script
  --host=localhost --ssl-mode=REQUIRED
  -uarthur -pabcd
  -x $query_simple_er_expired_change_pwd_file 2>&1;

# Try using fast auth - Must fail
--error 1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uarthur -pabcd -e "SELECT CURRENT_USER()" 2>&1

# Must succeed with fast auth now - compliant password format
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uarthur -pabcd -e "SELECT CURRENT_USER()" 2>&1

# Must succeed with fast auth now - compliant password format
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uarthur -pabcd -e "SELECT CURRENT_USER()" 2>&1

# Try connecing with another password stored in non-compliant format and reset password
let TEST_PARAM_PASS=efgh;
exec $parameter_client_script
  --host=localhost --ssl-mode=REQUIRED
  -umarvin -pefgh
  -x $query_simple_er_expired_change_pwd_file 2>&1;


# Try using fast auth - Must fail
--error 1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -umarvin -pefgh -e "SELECT CURRENT_USER()" 2>&1

# Must succeed with fast auth now - compliant password format
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -umarvin -pefgh -e "SELECT CURRENT_USER()" 2>&1

# Must succeed with fast auth now - compliant password format
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -umarvin -pefgh -e "SELECT CURRENT_USER()" 2>&1


# -------------------------------------------------------------------

# Drop users
DROP USER arthur, marvin, zaphod, ford;

# Revert to initial setting
SET GLOBAL caching_sha2_password_enforce_storage_format=@saved_enforce_storage_format;
SET GLOBAL caching_sha2_password_storage_format=@saved_storage_format;

--remove_file $query_account_and_change_pwd_file
--remove_file $query_account_file
--remove_file $query_simple_file
--remove_file $query_simple_er_expired_file
--remove_file $query_simple_er_expired_change_pwd_file

--let $parameter_client_script=
--let $parameter_client=

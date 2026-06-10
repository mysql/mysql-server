# Test the control variables for caching_sha2_password plugin.
# Those variables are: caching_sha2_password_storage_format,
# caching_sha2_password_enforce_storage_format.
#
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
# $parameter_test_second_password -
#    Run the test part that operates on secondary account password
#
## Usage
#
# --let $parameter_client_script = $MYSQL_TEST --connect-expired-password
# --let $parameter_client = $MYSQL --connect-expired-password
# --let $parameter_test_second_password = 0
# --source suite/auth_sec/include/plugin_auth_caching_sha2_password_multi_format_support.inc
#

if (`SELECT '$parameter_client_script' = ''`)
{
  --die "plugin_auth_caching_sha2_password_multi_format_support.inc" requires "parameter_client_script" parameter.
}

if (`SELECT '$parameter_client' = ''`)
{
  --die "plugin_auth_caching_sha2_password_multi_format_support.inc" requires "parameter_client" parameter.
}

if (`SELECT '$parameter_test_second_password' = ''`)
{
  --die "plugin_auth_caching_sha2_password_multi_format_support.inc" requires "parameter_test_second_password" parameter.
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

# -------------------------------------------------------------------
# The test begins here

# Accounts with CRYPT5
CREATE USER arthur IDENTIFIED WITH caching_sha2_password BY 'abcd';
CREATE USER marvin IDENTIFIED WITH caching_sha2_password BY 'efgh';

# Accounts with PBKDF2_SHA512
SET GLOBAL caching_sha2_password_storage_format="PBKDF2_SHA512";

CREATE USER zaphod IDENTIFIED WITH caching_sha2_password BY 'ijkl';
CREATE USER ford IDENTIFIED WITH caching_sha2_password BY 'mnop';

GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO arthur, marvin, zaphod, ford;

SELECT user, SUBSTRING(authentication_string, 1, 7) as format FROM mysql.user WHERE user IN ('arthur', 'marvin', 'zaphod', 'ford') ORDER BY user ASC;

# Try connecting - Must succeed
exec $parameter_client_script
  --host=127.0.0.1 --ssl-mode=REQUIRED
  -uarthur -pabcd
  -x $query_account_file 2>&1;

# Must succeed without TLS
# Set password - Must use PBKDF2_SHA512 format
let TEST_PARAM_PASS=abcde;
exec $parameter_client_script
  --host=localhost --ssl-mode=DISABLED
  -uarthur -pabcd
  -x $query_account_and_change_pwd_file 2>&1;

SELECT SUBSTRING(authentication_string, 1, 7) FROM mysql.user WHERE user = 'arthur';

# Try connecting - Must succeed
--exec $parameter_client --ssl-mode=REQUIRED -uarthur -pabcde -e "SELECT CURRENT_USER()" 2>&1

# Must succeed without TLS
--exec $parameter_client --ssl-mode=DISABLED -uarthur -pabcde -e "SELECT CURRENT_USER()" 2>&1

# Try connecting - Must succeed
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uzaphod -pijkl -e "SELECT CURRENT_USER()" 2>&1

# Must succeed without TLS
let TEST_PARAM_PASS=ijklm;
exec $parameter_client_script
  --host=localhost --ssl-mode=DISABLED
  -uzaphod -pijkl
  -x $query_account_and_change_pwd_file 2>&1;

SELECT SUBSTRING(authentication_string, 1, 7) FROM mysql.user WHERE user = 'zaphod';

# Change format
SET GLOBAL caching_sha2_password_storage_format="CRYPT5";

FLUSH PRIVILEGES;

# Try connecting - Must succeed
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uzaphod -pijklm -e "SELECT CURRENT_USER()" 2>&1

# Must succeed without TLS
let TEST_PARAM_PASS=ijkl;
exec $parameter_client_script
  --host=localhost --ssl-mode=DISABLED
  -uzaphod -pijklm
  -x $query_account_and_change_pwd_file 2>&1;

# At this point we have
# arthur, ford - Using PBKDF2_SHA512
# marvin, zaphod - Using CRYPT5

SELECT user, SUBSTRING(authentication_string, 1, 7) as format FROM mysql.user WHERE user IN ('arthur', 'marvin', 'zaphod', 'ford') ORDER BY user ASC;

# Dual password

DROP USER arthur, marvin, zaphod, ford;

SET GLOBAL caching_sha2_password_storage_format="CRYPT5";

# Accounts with CRYPT5
CREATE USER arthur IDENTIFIED WITH caching_sha2_password BY 'abcd';
CREATE USER marvin IDENTIFIED WITH caching_sha2_password BY 'efgh';

# Accounts with PBKDF2_SHA512
SET GLOBAL caching_sha2_password_storage_format="PBKDF2_SHA512";

CREATE USER zaphod IDENTIFIED WITH caching_sha2_password BY 'ijkl';
CREATE USER ford IDENTIFIED WITH caching_sha2_password BY 'mnop';

# Set second password
ALTER USER arthur IDENTIFIED BY 'efgh' RETAIN CURRENT PASSWORD;
ALTER USER marvin IDENTIFIED BY 'abcd' RETAIN CURRENT PASSWORD;

SET GLOBAL caching_sha2_password_storage_format="CRYPT5";

ALTER USER zaphod IDENTIFIED BY 'mnop' RETAIN CURRENT PASSWORD;
ALTER USER ford IDENTIFIED BY 'ijkl' RETAIN CURRENT PASSWORD;

GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO arthur, marvin, zaphod, ford;

# Current state
# User    CRYPT5        PBKDF2_SHA512
# arthur   abcd(2nd)     efgh(1st)
# marvin  efgh(2nd)     abcd(1st)
# zaphod  mnop(1st)     ijkl(2nd)
# ford    ijkl(1st)     mnop(2nd)

SELECT user, SUBSTRING(authentication_string, 1, 7) as first, SUBSTRING(JSON_EXTRACT(User_attributes, "$.additional_password"), 2, 7) as second FROM mysql.user WHERE user IN ('arthur', 'marvin', 'zaphod', 'ford') ORDER BY user ASC;

# Try with first password
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uarthur -pefgh -e "SELECT CURRENT_USER()" 2>&1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -umarvin -pabcd -e "SELECT CURRENT_USER()" 2>&1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uzaphod -pmnop -e "SELECT CURRENT_USER()" 2>&1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uford -pijkl -e "SELECT CURRENT_USER()" 2>&1

if ($parameter_test_second_password)
{
  # Try with second password
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uarthur -pabcd -e "SELECT CURRENT_USER()" 2>&1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -umarvin -pefgh -e "SELECT CURRENT_USER()" 2>&1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uzaphod -pijkl -e "SELECT CURRENT_USER()" 2>&1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uford -pmnop -e "SELECT CURRENT_USER()" 2>&1
}

FLUSH PRIVILEGES;

SET GLOBAL caching_sha2_password_storage_format="PBKDF2_SHA512";

# Try with first password
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uarthur -pefgh -e "SELECT CURRENT_USER()" 2>&1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -umarvin -pabcd -e "SELECT CURRENT_USER()" 2>&1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uzaphod -pmnop -e "SELECT CURRENT_USER()" 2>&1
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uford -pijkl -e "SELECT CURRENT_USER()" 2>&1

if ($parameter_test_second_password)
{
  # Try with second password
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uarthur -pabcd -e "SELECT CURRENT_USER()" 2>&1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -umarvin -pefgh -e "SELECT CURRENT_USER()" 2>&1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uzaphod -pijkl -e "SELECT CURRENT_USER()" 2>&1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uford -pmnop -e "SELECT CURRENT_USER()" 2>&1
}

# Tests for caching_sha2_password_enforce_format

# Current state
# User    CRYPT5        PBKDF2_SHA512
# arthur   abcd(2nd)     efgh(1st)
# marvin  efgh(2nd)     abcd(1st)
# zaphod  mnop(1st)     ijkl(2nd)
# ford    ijkl(1st)     mnop(2nd)

SET GLOBAL caching_sha2_password_storage_format="PBKDF2_SHA512";
SET GLOBAL caching_sha2_password_enforce_storage_format=1;

if ($parameter_test_second_password)
{
  # Try connecting with password stored in non-compliant format
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uarthur -pabcd -e "SELECT CURRENT_USER()" 2>&1

  # Try using fast auth - Must fail
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uarthur -pabcd -e "SELECT CURRENT_USER()" 2>&1
}

# Try connecing with password stored in compliant format
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -umarvin -pabcd -e "SELECT 1" 2>&1

# Must succeed
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -umarvin -pabcd -e "SELECT CURRENT_USER()" 2>&1

# Try with another password in compliant format
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uarthur -pefgh -e "SELECT 1" 2>&1

# Must succeed
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uarthur -pefgh -e "SELECT CURRENT_USER()" 2>&1


if ($parameter_test_second_password)
{
  # Try connecing with another password stored in non-compliant format
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -umarvin -pefgh -e "SELECT CURRENT_USER()" 2>&1

  # Try using fast auth - Must fail
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -umarvin -pefgh -e "SELECT CURRENT_USER()" 2>&1
}

if ($parameter_test_second_password)
{
  # Try connecting with password stored in non-compliant format and reset password
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uarthur -pabcd -e "SELECT CURRENT_USER()" 2>&1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uarthur -pabcd -e "SET PASSWORD='abcd'" 2>&1

  # Try using fast auth - Must fail
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uarthur -pabcd -e "SELECT CURRENT_USER()" 2>&1

  # Must succeed - secure channel
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uarthur -pabcd -e "SELECT CURRENT_USER()" 2>&1

  # Must succeed with fast auth now - compliant password format
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uarthur -pabcd -e "SELECT CURRENT_USER()" 2>&1

  # Try connecing with another password stored in non-compliant format and reset password
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -umarvin -pefgh -e "SELECT CURRENT_USER()" 2>&1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -umarvin -pefgh -e "SET PASSWORD='efgh'" 2>&1

  # Try using fast auth - Must fail
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -umarvin -pefgh -e "SELECT CURRENT_USER()" 2>&1

  # Must succeed - secure channel
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -umarvin -pefgh -e "SELECT CURRENT_USER()" 2>&1

  # Must succeed with fast auth now - compliant password format
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -umarvin -pefgh -e "SELECT CURRENT_USER()" 2>&1
}

SET GLOBAL caching_sha2_password_storage_format="CRYPT5";
SET GLOBAL caching_sha2_password_enforce_storage_format=1;

if ($parameter_test_second_password)
{
  # Try connecting with password stored in non-compliant format
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uzaphod -pijkl -e "SELECT CURRENT_USER()" 2>&1

  # Try using fast auth - Must fail
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uzaphod -pijkl -e "SELECT CURRENT_USER()" 2>&1
}

# Try connecing with password stored in compliant format

--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uford -pijkl -e "SELECT 1" 2>&1

# Must succeed
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uford -pijkl -e "SELECT CURRENT_USER()" 2>&1

# Try with another password in compliant format
--exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uzaphod -pmnop -e "SELECT 1" 2>&1

# Must succeed
--exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uzaphod -pmnop -e "SELECT CURRENT_USER()" 2>&1

if ($parameter_test_second_password)
{
  # Try connecing with another password stored in non-compliant format
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uford -pmnop -e "SELECT CURRENT_USER()" 2>&1

  # Try using fast auth - Must fail
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uford -pmnop -e "SELECT CURRENT_USER()" 2>&1

  # Try connecting with password stored in non-compliant format and reset password
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uzaphod -pijkl -e "SELECT CURRENT_USER()" 2>&1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uzaphod -pijkl -e "SET PASSWORd='ijkl'" 2>&1

  # Try using fast auth - Must fail
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uzaphod -pijkl -e "SELECT CURRENT_USER()" 2>&1

  # Must succeed with fast auth now - compliant password format
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uzaphod -pijkl -e "SELECT CURRENT_USER()" 2>&1

  # Must succeed with fast auth now - compliant password format
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uzaphod -pijkl -e "SELECT CURRENT_USER()" 2>&1

  # Try connecing with another password stored in non-compliant format and reset password
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uford -pmnop -e "SELECT CURRENT_USER()" 2>&1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uford -pmnop -e "SET PASSWORD='mnop'" 2>&1


  # Try using fast auth - Must fail
  --error 1
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uford -pmnop -e "SELECT CURRENT_USER()" 2>&1

  # Must succeed with fast auth now - compliant password format
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=REQUIRED -uford -pmnop -e "SELECT CURRENT_USER()" 2>&1

  # Must succeed with fast auth now - compliant password format
  --exec $parameter_client --host=127.0.0.1 --ssl-mode=DISABLED -uford -pmnop -e "SELECT CURRENT_USER()" 2>&1
}

# -------------------------------------------------------------------

# Drop users
DROP USER arthur, marvin, zaphod, ford;

# Revert to initial setting
SET GLOBAL caching_sha2_password_enforce_storage_format=@saved_enforce_storage_format;
SET GLOBAL caching_sha2_password_storage_format=@saved_storage_format;

--remove_file $query_account_and_change_pwd_file
--remove_file $query_account_file
--remove_file $query_simple_file

--let $parameter_client_script=
--let $parameter_client=
--let $parameter_test_second_password=

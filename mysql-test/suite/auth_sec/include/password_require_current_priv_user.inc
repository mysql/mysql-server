###############################################################################
#                                                                             #
#  This script checks the ALTER USER|SET PASSWORD ... REPLACE ... SQL         #
#  for privileged user.                                                       #
#                                                                             #
#  This script expects, two users have been created already by the caller.    #
#  It grants the update on mysql.* privileges to one user to treat that as    #
#  privileged user, while other user is not granted any privileges.           #
#  It does not drop the users.                                                #
#                                                                             #
#   Arguments required:                                                       #
#   It needs to know the two users' names through following variables:        #
#   1. $PRIV_USER : user which will be granted the update privileges          #
#   2. $NON_PRIV_USER : user which will not be granted the update privileges  #
#                                                                             #
#   It also needs following two password variables:                           #
#   3. $PWRD_1:  A password which is used to create $NON_PRIV_USER user.      #
#   4. $PWRD_2:  A password which is used to create $PRIV_USER user.          #
#                                                                             #
###############################################################################

# If required parameters are not set then throw error and exit.
--let PARAM_PERL_QUIT=0
if ($NON_PRIV_USER == '')
{
--let VARIABLE_NAME = NON_PRIV_USER
--let PARAM_PERL_QUIT=1
}

if ($PRIV_USER == '')
{
--let VARIABLE_NAME = PRIV_USER
--let PARAM_PERL_QUIT=1
}

if ($PWRD_1 == '')
{
--let VARIABLE_NAME = PWRD_1
--let PARAM_PERL_QUIT=1
}

if ($PWRD_2 == '')
{
--let VARIABLE_NAME = PWRD_2
--let PARAM_PERL_QUIT=1
}

--let $WRONG_PWRD = P&(*^_)D

--echo
--echo # Show how the users were created
--echo
--replace_regex /WITH '(.*)' AS '(.*)' REQUIRE/WITH '<default_auth_plugin>' AS '<password_hash>' REQUIRE/
eval SHOW CREATE USER $NON_PRIV_USER;
--replace_regex /WITH '(.*)' AS '(.*)' REQUIRE/WITH '<default_auth_plugin>' AS '<password_hash>' REQUIRE/
eval SHOW CREATE USER $PRIV_USER;
eval SELECT password_require_current from mysql.user
     where user like '%$PRIV_USER' OR user like '%$NON_PRIV_USER';

--echo
--echo # Grant the update privileges to the user
eval GRANT UPDATE ON mysql.* to '$PRIV_USER'@'%' WITH GRANT OPTION;

--echo
--echo #------------------------------------------------------------------------
--echo # Priv user changing his own password
--echo #------------------------------------------------------------------------
connect(con1,localhost,$PRIV_USER,$PWRD_2,);

--echo
--echo # Without REPLACE clause
eval SET PASSWORD='$PWRD_1';
eval SET PASSWORD for $PRIV_USER='$PWRD_2';
eval ALTER USER user() IDENTIFIED BY '$PWRD_1';
eval ALTER USER $PRIV_USER IDENTIFIED BY '$PWRD_2';

--echo
--echo # With invalid current password in the REPLACE clause
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD='$PWRD_2' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD for $PRIV_USER='$PWRD_2' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER user() IDENTIFIED BY '$PWRD_2' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER $PRIV_USER IDENTIFIED BY '$PWRD_2' REPLACE '$WRONG_PWRD';

--echo
--echo # With valid current password in the REPLACE clause
eval SET PASSWORD='$PWRD_1' REPLACE '$PWRD_2';
eval SET PASSWORD for $PRIV_USER='$PWRD_2' REPLACE '$PWRD_1';
eval ALTER USER user() IDENTIFIED BY '$PWRD_1' REPLACE '$PWRD_2';
eval ALTER USER $PRIV_USER IDENTIFIED BY '$PWRD_2' REPLACE '$PWRD_1';

--echo
--echo #------------------------------------------------------------------------
--echo # Priv user changing the password of $NON_PRIV_USER
--echo #------------------------------------------------------------------------

--echo
--error ER_CURRENT_PASSWORD_NOT_REQUIRED
eval SET PASSWORD for $NON_PRIV_USER='$PWRD_2' REPLACE '$PWRD_1';
--error ER_CURRENT_PASSWORD_NOT_REQUIRED
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_2' REPLACE '';

--echo
eval SET PASSWORD for $NON_PRIV_USER='$PWRD_2';
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_2';

connection default;
disconnect con1;

--echo
--echo #------------------------------------------------------------------------
--echo # One priv user(root) changing password for other priv user ($PRIV_USER)
--echo #------------------------------------------------------------------------

--echo
--error ER_CURRENT_PASSWORD_NOT_REQUIRED
eval SET PASSWORD for $PRIV_USER='$PWRD_1' REPLACE '$PWRD_2';
--error ER_CURRENT_PASSWORD_NOT_REQUIRED
eval ALTER USER $PRIV_USER IDENTIFIED BY '$PWRD_1' REPLACE '$WRONG_PWRD';

--echo
eval ALTER USER $PRIV_USER IDENTIFIED BY '$PWRD_2';
eval SET PASSWORD for $PRIV_USER='$PWRD_1';

--echo
--echo # Now test with Global server variable ON
SET @@global.password_require_current=ON;

--echo
--echo #------------------------------------------------------------------------
--echo # Priv user changing his own password
--echo #------------------------------------------------------------------------
connect(con1,localhost,$PRIV_USER,$PWRD_1,);

--echo
--echo # Can change without REPLACE clause
eval SET PASSWORD='$PWRD_2';
eval SET PASSWORD for $PRIV_USER='$PWRD_1';
eval ALTER USER user() IDENTIFIED BY '$PWRD_2';
eval ALTER USER $PRIV_USER IDENTIFIED BY '$PWRD_1';

--echo
--echo # Incorrect password specified in the REPLACE clause
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD='$PWRD_2' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD for $PRIV_USER='$PWRD_2' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER user() IDENTIFIED BY '$PWRD_2' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER $PRIV_USER IDENTIFIED BY '$PWRD_2' REPLACE '$WRONG_PWRD';

--echo
--echo # Correct password specified in the REPLACE clause
eval SET PASSWORD='$PWRD_2' REPLACE '$PWRD_1';
eval SET PASSWORD for $PRIV_USER='$PWRD_1' REPLACE '$PWRD_2';
eval ALTER USER user() IDENTIFIED BY '$PWRD_2' REPLACE '$PWRD_1';
eval ALTER USER $PRIV_USER IDENTIFIED BY '$PWRD_1' REPLACE '$PWRD_2'
     WITH MAX_QUERIES_PER_HOUR 100 PASSWORD EXPIRE DEFAULT;

--echo
--echo #------------------------------------------------------------------------
--echo # Priv user changing the password of $NON_PRIV_USER
--echo #------------------------------------------------------------------------

--echo
--error ER_CURRENT_PASSWORD_NOT_REQUIRED
eval SET PASSWORD for $NON_PRIV_USER='$PWRD_2' REPLACE '$WRONG_PWRD';
--error ER_CURRENT_PASSWORD_NOT_REQUIRED
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_2' REPLACE '$WRONG_PWRD';

--echo
eval SET PASSWORD for $NON_PRIV_USER='$PWRD_2' ;
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1';

connection default;
disconnect con1;

--echo
--echo #------------------------------------------------------------------------
--echo # One priv user(root) changing password for other priv user ($PRIV_USER)
--echo #------------------------------------------------------------------------

--echo
--error ER_CURRENT_PASSWORD_NOT_REQUIRED
eval SET PASSWORD for $PRIV_USER='$PWRD_1' REPLACE '';
--error ER_CURRENT_PASSWORD_NOT_REQUIRED
eval ALTER USER $PRIV_USER IDENTIFIED BY '$PWRD_1' REPLACE '$PWRD_2';
eval SET PASSWORD for $PRIV_USER='$PWRD_1';
eval ALTER USER $PRIV_USER IDENTIFIED BY '$PWRD_2';

--echo
--echo # Reset the privileges and global variable
--echo
eval REVOKE UPDATE ON mysql.* FROM '$PRIV_USER'@'%';
SET @@global.password_require_current=OFF;
###############################################################################
#                                                                             #
#  This script checks the ALTER USER|SET PASSWORD ... REPLACE ... SQL         #
#  for non privileged user.                                                   #
#                                                                             #
#  This include file expects, two users 'priv_usr' and 'non_priv_usr'         #
#  already created before it is called. It does not drop the users.           #
#                                                                             #
#   It expect the following two variables to be set                           #
#   1. $NON_PRIV_USER : user which will not be granted the update privileges  #
#                                                                             #
#   2. $CURRENT_PASSWORD_REQUIRE : What clause was used to create             #
#                                  the $NON_PRIV_USER                         #
#           CURRENT  ~ PASSWORD REQUIRE CURRENT                               #
#           OPTIONAL ~ PASSWORD REQUIRE CURRENT OPTIONAL                      #
#           DEFAULT  ~ PASSWORD REQUIRE CURRENT DEFAULT                       #
#                                                                             #
#   It also needs following two password variables:                           #
#   3. $PWRD_1:  A password which is used to create $NON_PRIV_USER user.      #
#                                                                             #
###############################################################################

# If required parameters are not set then throw error and exit.
--let PARAM_PERL_QUIT=0
if ($NON_PRIV_USER == '')
{
--let VARIABLE_NAME = NON_PRIV_USER
--let PARAM_PERL_QUIT=1
}

if ($CURRENT_PASSWORD_REQUIRE == '')
{
--let VARIABLE_NAME = CURRENT_PASSWORD_REQUIRE
--let PARAM_PERL_QUIT=1
}

--perl
    $variable_name = "$ENV{VARIABLE_NAME}";
    die "Error: password_require_current.inc: input variable $variable_name is missing!"
             if ($ENV{PARAM_PERL_QUIT} == 1);
EOF

--let $TEMP_PWRD = pwd2
--let $WRONG_PWRD = P&(*^_)D

--echo
--echo #------------------------------------------------------------------------
--echo # Non priv user changing his own password
--echo #------------------------------------------------------------------------
connect(con1,localhost,$NON_PRIV_USER,$PWRD_1,);

--echo
--echo # REPLACE clause is not specified
if($CURRENT_PASSWORD_REQUIRE == 'CURRENT')
{
--error ER_MISSING_CURRENT_PASSWORD
eval SET PASSWORD='$TEMP_PWRD';
--error ER_MISSING_CURRENT_PASSWORD
eval SET PASSWORD for $NON_PRIV_USER='$TEMP_PWRD';
--error ER_MISSING_CURRENT_PASSWORD
eval ALTER USER user() IDENTIFIED BY '$PWRD_1';
--error ER_MISSING_CURRENT_PASSWORD
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1';
}
if($CURRENT_PASSWORD_REQUIRE == 'OPTIONAL')
{
eval SET PASSWORD='$TEMP_PWRD';
eval SET PASSWORD for $NON_PRIV_USER='$TEMP_PWRD';
eval ALTER USER user() IDENTIFIED BY '$PWRD_1';
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1';
}
if($CURRENT_PASSWORD_REQUIRE == 'DEFAULT')
{
eval SET PASSWORD='$TEMP_PWRD';
eval SET PASSWORD for $NON_PRIV_USER='$TEMP_PWRD';
eval ALTER USER user() IDENTIFIED BY '$PWRD_1';
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1';
}

--echo
--echo # Incorrect password specified in the REPLACE clause
if($CURRENT_PASSWORD_REQUIRE == 'CURRENT')
{
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD='$TEMP_PWRD' REPLACE '$WRONG_PWRD' ;
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD for $NON_PRIV_USER='$TEMP_PWRD' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER user() IDENTIFIED BY '$TEMP_PWRD' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$TEMP_PWRD' REPLACE '$WRONG_PWRD';
}
if($CURRENT_PASSWORD_REQUIRE == 'OPTIONAL')
{
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD='$TEMP_PWRD' REPLACE '$WRONG_PWRD' ;
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD for $NON_PRIV_USER='$TEMP_PWRD' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER user() IDENTIFIED BY '$TEMP_PWRD' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1' REPLACE '$WRONG_PWRD';
}
if($CURRENT_PASSWORD_REQUIRE == 'DEFAULT')
{
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD='$TEMP_PWRD' REPLACE '' ;
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD for $NON_PRIV_USER='$TEMP_PWRD' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER user() IDENTIFIED BY '$TEMP_PWRD' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1' REPLACE '$WRONG_PWRD';
}

--echo
--echo # Correct password specified in the REPLACE clause
if($CURRENT_PASSWORD_REQUIRE == 'CURRENT')
{
eval SET PASSWORD='$TEMP_PWRD' REPLACE '$PWRD_1';
eval SET PASSWORD for $NON_PRIV_USER='$PWRD_1' REPLACE '$TEMP_PWRD';
eval ALTER USER user() IDENTIFIED BY '$TEMP_PWRD' REPLACE '$PWRD_1';
eval ALTER USER IF EXISTS $NON_PRIV_USER IDENTIFIED BY '$PWRD_1' REPLACE '$TEMP_PWRD';
}
if($CURRENT_PASSWORD_REQUIRE == 'OPTIONAL')
{
eval SET PASSWORD='$TEMP_PWRD' REPLACE '$PWRD_1';
eval SET PASSWORD for $NON_PRIV_USER='$PWRD_1' REPLACE '$TEMP_PWRD';
eval ALTER USER user() IDENTIFIED BY '$TEMP_PWRD' REPLACE '$PWRD_1';
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1' REPLACE '$TEMP_PWRD';
}
if($CURRENT_PASSWORD_REQUIRE == 'DEFAULT')
{
eval SET PASSWORD='$TEMP_PWRD' REPLACE '$PWRD_1';
eval SET PASSWORD for $NON_PRIV_USER='$PWRD_1' REPLACE '$TEMP_PWRD';
eval ALTER USER user() IDENTIFIED BY '$TEMP_PWRD' REPLACE '$PWRD_1';
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1' REPLACE '$TEMP_PWRD';
}
connection default;
disconnect con1;

--echo
--echo # Now test with Global server variable ON
SET @@global.password_require_current=ON;

--echo
--echo #------------------------------------------------------------------------
--echo # Non priv user changing his own password
--echo #------------------------------------------------------------------------
connect(con1,localhost,$NON_PRIV_USER,$PWRD_1,);

--echo
--echo # REPLACE clause is not specified
if($CURRENT_PASSWORD_REQUIRE == 'CURRENT')
{
--error ER_MISSING_CURRENT_PASSWORD
eval SET PASSWORD='$TEMP_PWRD';
--error ER_MISSING_CURRENT_PASSWORD
eval SET PASSWORD for $NON_PRIV_USER='$TEMP_PWRD';
--error ER_MISSING_CURRENT_PASSWORD
eval ALTER USER user() IDENTIFIED BY '$TEMP_PWRD';
--error ER_MISSING_CURRENT_PASSWORD
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$TEMP_PWRD';
}
if($CURRENT_PASSWORD_REQUIRE == 'OPTIONAL')
{
eval SET PASSWORD='$TEMP_PWRD';
eval SET PASSWORD for $NON_PRIV_USER='$PWRD_1';
eval ALTER USER user() IDENTIFIED BY '$TEMP_PWRD';
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1';
}
if($CURRENT_PASSWORD_REQUIRE == 'DEFAULT')
{
--error ER_MISSING_CURRENT_PASSWORD
eval SET PASSWORD='$TEMP_PWRD';
--error ER_MISSING_CURRENT_PASSWORD
eval SET PASSWORD for $NON_PRIV_USER='$TEMP_PWRD';
--error ER_MISSING_CURRENT_PASSWORD
eval ALTER USER user() IDENTIFIED BY '$PWRD_1';
--error ER_MISSING_CURRENT_PASSWORD
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1';
}

--echo
--echo # Incorrect password specified in the REPLACE clause
--error ER_INCORRECT_CURRENT_PASSWORD
if($CURRENT_PASSWORD_REQUIRE == 'CURRENT')
{
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD='$TEMP_PWRD' REPLACE '$WRONG_PWRD' ;
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD for $NON_PRIV_USER='$TEMP_PWRD' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER user() IDENTIFIED BY '$TEMP_PWRD' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1' REPLACE '$WRONG_PWRD';
}
if($CURRENT_PASSWORD_REQUIRE == 'OPTIONAL')
{
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD='$TEMP_PWRD' REPLACE '$WRONG_PWRD' ;
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD for $NON_PRIV_USER='$TEMP_PWRD' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER user() IDENTIFIED BY '$TEMP_PWRD' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1' REPLACE '$WRONG_PWRD';
}
if($CURRENT_PASSWORD_REQUIRE == 'DEFAULT')
{
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD='$TEMP_PWRD' REPLACE '' ;
--error ER_INCORRECT_CURRENT_PASSWORD
eval SET PASSWORD for $NON_PRIV_USER='$TEMP_PWRD' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER user() IDENTIFIED BY '$TEMP_PWRD' REPLACE '$WRONG_PWRD';
--error ER_INCORRECT_CURRENT_PASSWORD
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1' REPLACE '$WRONG_PWRD';
}

--echo
--echo # Correct password specified in the REPLACE clause
if($CURRENT_PASSWORD_REQUIRE == 'CURRENT')
{
eval SET PASSWORD='$TEMP_PWRD' REPLACE '$PWRD_1';
eval SET PASSWORD for $NON_PRIV_USER='$PWRD_1' REPLACE '$TEMP_PWRD';
eval ALTER USER user() IDENTIFIED BY '$TEMP_PWRD' REPLACE '$PWRD_1';
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1' REPLACE '$TEMP_PWRD';
}
if($CURRENT_PASSWORD_REQUIRE == 'OPTIONAL')
{
eval SET PASSWORD='$TEMP_PWRD' REPLACE '$PWRD_1';
eval SET PASSWORD for $NON_PRIV_USER='$PWRD_1' REPLACE '$TEMP_PWRD';
eval ALTER USER user() IDENTIFIED BY '$TEMP_PWRD' REPLACE '$PWRD_1';
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1' REPLACE '$TEMP_PWRD';
}
if($CURRENT_PASSWORD_REQUIRE == 'DEFAULT')
{
eval SET PASSWORD='$TEMP_PWRD' REPLACE '$PWRD_1';
eval SET PASSWORD for $NON_PRIV_USER='$PWRD_1' REPLACE '$TEMP_PWRD';
eval ALTER USER user() IDENTIFIED BY '$TEMP_PWRD' REPLACE '$PWRD_1';
eval ALTER USER $NON_PRIV_USER IDENTIFIED BY '$PWRD_1' REPLACE '$TEMP_PWRD';
}

connection default;
disconnect con1;

--echo
--echo # Reset the Global server variable to OFF
SET @@global.password_require_current=OFF;
--echo

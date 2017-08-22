############## mysql-test\t\slave_transaction_retries_basic.test ################
#                                                                               #
# Variable Name: slave_transaction_retries                                      #
# Scope: GLOBAL                                                                 #
# Access Type: Dynamic                                                          #
# Data Type: numeric                                                            #
# Default Value: 10                                                             #
# Range: 0 - 4294967295/18446744073709551615                                    #
#                                                                               #
#                                                                               #
# Creation Date: 2008-02-07                                                     #
# Author:  Rizwan                                                               #
#                                                                               #
# Description: Test Cases of Dynamic System Variable slave_transaction_retries  #
#              that checks the behavior of this variable in the following ways  #
#              * Default Value                                                  #
#              * Valid & Invalid values                                         #
#              * Scope & Access method                                          #
#              * Data Integrity                                                 #
#                                                                               #
# Modified:    Pavan Naik, 2015-11-24                                           #
#              Added boundary value test cases according to the type of the     #
#              server binary.                                                   #
#                                                                               #
# Reference: http://dev.mysql.com/doc/refman/5.1/en/                            #
#            server-system-variables.html                                       #
#                                                                               #
#################################################################################

--source include/load_sysvars.inc

###################################################################
#           START OF slave_transaction_retries TESTS              #
###################################################################


#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.slave_transaction_retries;

--echo '#--------------------FN_DYNVARS_149_01-------------------------#'
###################################################################
#     Display the DEFAULT value of slave_transaction_retries      #
###################################################################

SET @@global.slave_transaction_retries = 50;
SET @@global.slave_transaction_retries = DEFAULT;
SELECT @@global.slave_transaction_retries;

--echo '#--------------------FN_DYNVARS_149_02-------------------------#'
###################################################################
#     Check the DEFAULT value of slave_transaction_retries         #
###################################################################

SET @@global.slave_transaction_retries = DEFAULT;
SELECT @@global.slave_transaction_retries = 10;

--echo '#--------------------FN_DYNVARS_149_03-------------------------#'
################################################################################
# Change the value of slave_transaction_retries to a valid value for
# GLOBAL Scope 
################################################################################

SET @@global.slave_transaction_retries = 0;
SELECT @@global.slave_transaction_retries;
SET @@global.slave_transaction_retries = 1;
SELECT @@global.slave_transaction_retries;
SET @@global.slave_transaction_retries = 15;
SELECT @@global.slave_transaction_retries;
SET @@global.slave_transaction_retries = 1024;
SELECT @@global.slave_transaction_retries;
SET @@global.slave_transaction_retries = 2147483648;
SELECT @@global.slave_transaction_retries;
SET @@global.slave_transaction_retries = 2147483648*2-1;
SELECT @@global.slave_transaction_retries;
SET @@global.slave_transaction_retries = 4294967295;
SELECT @@global.slave_transaction_retries;

# Test cases for 64-bit machines
if($mach64)
{
  --disable_warnings
  SET @@global.slave_transaction_retries = 18446744073709551615;
  --enable_warnings

  # Bug#11752618 - 64-BIT WINDOWS VARIABLES LIMITED TO DIFFERENT VALUES THAN 64-BIT LINUX/SO
  let $machine_name = `SELECT CONVERT(@@version_compile_os using latin1) IN ("Win64", "Windows")`;

  if($machine_name)
  {
    --replace_result 4294967295 18446744073709551615
  }
  SELECT @@global.slave_transaction_retries;
}

--echo '#--------------------FN_DYNVARS_149_04-------------------------#'
###############################################################################
#           Check if variable can be access with session scope                #
###############################################################################

--Error ER_GLOBAL_VARIABLE
SET @@slave_transaction_retries = 2;

--Error ER_GLOBAL_VARIABLE
SET @@session.slave_transaction_retries = 3;

--Error ER_GLOBAL_VARIABLE
SET @@local.slave_transaction_retries = 4;



--echo '#------------------FN_DYNVARS_149_05-----------------------#'
####################################################################
# Change the value of slave_transaction_retries to an invalid value#
####################################################################

SET @@global.slave_transaction_retries = -1;
SELECT @@global.slave_transaction_retries;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.slave_transaction_retries = 65530.34;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.slave_transaction_retries = '100';
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.slave_transaction_retries = 7483649.56;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.slave_transaction_retries = ON;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.slave_transaction_retries = OFF;

if($mach32)
{
  SET @@global.slave_transaction_retries = 4294967296;
  SELECT @@global.slave_transaction_retries;

  SET @@global.slave_transaction_retries = 42949672950;
  SELECT @@global.slave_transaction_retries;
}

if($mach64)
{
  --error ER_WRONG_TYPE_FOR_VAR
  SET @@global.slave_transaction_retries = 18446744073709551616;
  SELECT @@global.slave_transaction_retries;
}

--echo '#------------------FN_DYNVARS_149_06-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################

SET @@global.slave_transaction_retries = 3000;
--disable_warnings
SELECT @@global.slave_transaction_retries = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='slave_transaction_retries';
--enable_warnings


--echo '#------------------FN_DYNVARS_149_07-----------------------#'
###########################################################################
#  Check if the value is present in performance_schema.session_variables  #
###########################################################################

--disable_warnings
SELECT count(VARIABLE_VALUE) 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='slave_transaction_retries';
--enable_warnings


--echo '#------------------FN_DYNVARS_149_08-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.slave_transaction_retries = TRUE;
SELECT @@global.slave_transaction_retries;
SET @@global.slave_transaction_retries = FALSE;
SELECT @@global.slave_transaction_retries;


--echo '#---------------------FN_DYNVARS_149_09----------------------#'
###############################################################################
#  Check if accessing variable with and without GLOBAL point to same variable #
###############################################################################


SET @@global.slave_transaction_retries = 60*60;
SELECT @@slave_transaction_retries = @@global.slave_transaction_retries;


--echo '#---------------------FN_DYNVARS_149_10----------------------#'
###############################################################################
#   Check if slave_transaction_retries can be accessed without @@ sign
#   and scope 
###############################################################################

--Error ER_GLOBAL_VARIABLE
SET slave_transaction_retries = 2048;
--Error ER_BAD_FIELD_ERROR
SELECT slave_transaction_retries;

SELECT @@slave_transaction_retries;

#verifying another another syntax for setting value
SET global slave_transaction_retries = 99;

####################################
#     Restore initial value        #
####################################

SET @@global.slave_transaction_retries = @start_global_value;

########################################################
#              END OF slave_transaction_retries TESTS  #
########################################################


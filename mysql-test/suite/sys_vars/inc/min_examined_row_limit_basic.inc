################ mysql-test\t\min_examined_row_limit_basic.test #################
#                                                                               #
# Variable Name: min_examined_row_limit                                         #
# Scope: GLOBAL | SESSION                                                       #
# Access Type: Dynamic                                                          #
# Data Type: numeric                                                            #
# Default Value: 0                                                              #
# Range: 0-4294967295/18446744073709551615                                      #
#                                                                               #
#                                                                               #
# Creation Date: 2008-02-07                                                     #
# Author:  Salman                                                               #
#                                                                               #
# Description: Test Cases of Dynamic System Variable min_examined_row_limit     #
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

######################################################################
#           START OF min_examined_row_limit TESTS                    #
######################################################################


#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.min_examined_row_limit;
SELECT @start_global_value;
SET @start_session_value = @@session.min_examined_row_limit;
SELECT @start_session_value;


--echo '#--------------------FN_DYNVARS_089_01-------------------------#'
######################################################################
#     Display the DEFAULT value of min_examined_row_limit            #
######################################################################

SET @@global.min_examined_row_limit = 100;
SET @@global.min_examined_row_limit = DEFAULT;
SELECT @@global.min_examined_row_limit;

SET @@session.min_examined_row_limit = 200;
SET @@session.min_examined_row_limit = DEFAULT;
SELECT @@session.min_examined_row_limit;


--echo '#--------------------FN_DYNVARS_089_02-------------------------#'
########################################################################
#     Check the DEFAULT value of min_examined_row_limit              #
########################################################################

SET @@global.min_examined_row_limit = DEFAULT;
SELECT @@global.min_examined_row_limit = 0;

SET @@session.min_examined_row_limit = DEFAULT;
SELECT @@session.min_examined_row_limit = 0;


--echo '#--------------------FN_DYNVARS_089_03-------------------------#'
################################################################################
# Change the value of min_examined_row_limit to a valid value for GLOBAL Scope #
################################################################################

SET @@global.min_examined_row_limit = 0;
SELECT @@global.min_examined_row_limit;
SET @@global.min_examined_row_limit = 1;
SELECT @@global.min_examined_row_limit;
SET @@global.min_examined_row_limit = 60020;
SELECT @@global.min_examined_row_limit;
SET @@global.min_examined_row_limit = 65535;
SELECT @@global.min_examined_row_limit;
SET @@global.min_examined_row_limit = 4294967295;
SELECT @@global.min_examined_row_limit;
SET @@global.min_examined_row_limit = 4294967294;
SELECT @@global.min_examined_row_limit;

# Test cases for 64-bit machines
if($mach64)
{
  --disable_warnings
  SET @@global.min_examined_row_limit = 18446744073709551615;
  --enable_warnings

  # Bug#11752618 - 64-BIT WINDOWS VARIABLES LIMITED TO DIFFERENT VALUES THAN 64-BIT LINUX/SO
  let $machine_name = `SELECT CONVERT(@@version_compile_os using latin1) IN ("Win64", "Windows")`;

  if($machine_name)
  {
    --replace_result 4294967295 18446744073709551615
  }
  SELECT @@global.min_examined_row_limit;
}


--echo '#--------------------FN_DYNVARS_089_04-------------------------#'
###################################################################################
# Change the value of min_examined_row_limit to a valid value for SESSION Scope #
###################################################################################
 
SET @@session.min_examined_row_limit = 0;
SELECT @@session.min_examined_row_limit;
SET @@session.min_examined_row_limit = 1;
SELECT @@session.min_examined_row_limit;
SET @@session.min_examined_row_limit = 50050;
SELECT @@session.min_examined_row_limit;
SET @@session.min_examined_row_limit = 65535;
SELECT @@session.min_examined_row_limit;
SET @@session.min_examined_row_limit = 4294967295;
SELECT @@session.min_examined_row_limit;
SET @@session.min_examined_row_limit = 4294967294;
SELECT @@session.min_examined_row_limit;

# Test cases for 64-bit machines
if($mach64)
{
  --disable_warnings
  SET @@session.min_examined_row_limit = 18446744073709551615;
  --enable_warnings

  # Bug#11752618 - 64-BIT WINDOWS VARIABLES LIMITED TO DIFFERENT VALUES THAN 64-BIT LINUX/SO
  if($machine_name)
  {
    --replace_result 4294967295 18446744073709551615
  }
  SELECT @@session.min_examined_row_limit;
}

--echo '#------------------FN_DYNVARS_089_05-----------------------#'
####################################################################
# Change the value of min_examined_row_limit to an invalid value #
####################################################################

SET @@global.min_examined_row_limit = -1024;
SELECT @@global.min_examined_row_limit;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.min_examined_row_limit = 65530.34;
SELECT @@global.min_examined_row_limit;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.min_examined_row_limit = test;
SELECT @@global.min_examined_row_limit;

if($mach32)
{
  SET @@global.min_examined_row_limit = 4294967296;
  SELECT @@global.min_examined_row_limit;

  SET @@global.min_examined_row_limit = 42949672950;
  SELECT @@global.min_examined_row_limit;
}

if($mach64)
{
  --error ER_WRONG_TYPE_FOR_VAR
  SET @@global.min_examined_row_limit = 18446744073709551616;
  SELECT @@global.min_examined_row_limit;
}

SET @@session.min_examined_row_limit = -1;
SELECT @@session.min_examined_row_limit;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.min_examined_row_limit = 65530.34;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.min_examined_row_limit = test;
SELECT @@session.min_examined_row_limit;

if($mach32)
{
  SET @@session.min_examined_row_limit = 4294967296;
  SELECT @@session.min_examined_row_limit;

  SET @@session.min_examined_row_limit = 42949672950;
  SELECT @@session.min_examined_row_limit;
}

if($mach64)
{
  --error ER_WRONG_TYPE_FOR_VAR
  SET @@session.min_examined_row_limit = 18446744073709551616;
  SELECT @@session.min_examined_row_limit;
}

--echo '#------------------FN_DYNVARS_089_06-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################

--disable_warnings
SELECT @@global.min_examined_row_limit = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='min_examined_row_limit';
--enable_warnings

--echo '#------------------FN_DYNVARS_089_07-----------------------#'
####################################################################
#  Check if the value in SESSION Table matches value in variable   #
####################################################################

--disable_warnings
SELECT @@session.min_examined_row_limit = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='min_examined_row_limit';
--enable_warnings

--echo '#------------------FN_DYNVARS_089_08-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.min_examined_row_limit = TRUE;
SELECT @@global.min_examined_row_limit;
SET @@global.min_examined_row_limit = FALSE;
SELECT @@global.min_examined_row_limit;


--echo '#---------------------FN_DYNVARS_089_09----------------------#'
#################################################################################### 
#  Check if accessing variable with and without GLOBAL point to same variable   #
#################################################################################### 

SET @@global.min_examined_row_limit = 10;
SELECT @@min_examined_row_limit = @@global.min_examined_row_limit;


--echo '#---------------------FN_DYNVARS_089_10----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@min_examined_row_limit = 100;
SELECT @@min_examined_row_limit = @@local.min_examined_row_limit;
SELECT @@local.min_examined_row_limit = @@session.min_examined_row_limit;


--echo '#---------------------FN_DYNVARS_089_11----------------------#'
###################################################################################  
#   Check if min_examined_row_limit can be accessed with and without @@ sign    #
###################################################################################

SET min_examined_row_limit = 1;
SELECT @@min_examined_row_limit;
--Error ER_UNKNOWN_TABLE
SELECT local.min_examined_row_limit;
--Error ER_UNKNOWN_TABLE
SELECT session.min_examined_row_limit;
--Error ER_BAD_FIELD_ERROR
SELECT min_examined_row_limit = @@session.min_examined_row_limit;


####################################
#     Restore initial value        #
####################################

SET @@global.min_examined_row_limit = @start_global_value;
SELECT @@global.min_examined_row_limit;
SET @@session.min_examined_row_limit = @start_session_value;
SELECT @@session.min_examined_row_limit;


#############################################################
#                 END OF min_examined_row_limit TESTS     #
#############################################################


############### mysql-test\t\myisam_repair_threads_basic.test ###################
#                                                                               #
# Variable Name: myisam_repair_threads                                          #
# Scope: GLOBAL | SESSION                                                       #
# Access Type: Dynamic                                                          #
# Data Type: numeric                                                            #
# Default Value: 1                                                              #
# Range: 1 - 4294967295/18446744073709551615                                    #
#                                                                               #
#                                                                               #
# Creation Date: 2008-02-07                                                     #
# Author:  Sharique Abdullah                                                    #
#                                                                               #
# Description: Test Cases of Dynamic System Variable myisam_repair_threads      #
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
#           START OF myisam_repair_threads  TESTS                    #
######################################################################


#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.myisam_repair_threads;
SELECT @start_global_value;
SET @start_session_value = @@session.myisam_repair_threads;
SELECT @start_session_value;


--echo '#--------------------FN_DYNVARS_005_01-------------------------#'
#################################################################
#     Display the DEFAULT value of myisam_block_size            #
#################################################################

SET @@global.myisam_repair_threads  = 100;
SET @@global.myisam_repair_threads  = DEFAULT;
SELECT @@global.myisam_repair_threads ;

SET @@session.myisam_repair_threads  = 200;
SET @@session.myisam_repair_threads  = DEFAULT;
SELECT @@session.myisam_repair_threads ;


--echo '#--------------------FN_DYNVARS_005_02-------------------------#'
######################################################################
#     Check the DEFAULT value of myisam_repair_threads               #
######################################################################

SET @@global.myisam_repair_threads  = DEFAULT;
SELECT @@global.myisam_repair_threads  = 1;

SET @@session.myisam_repair_threads  = DEFAULT;
SELECT @@session.myisam_repair_threads  = 1;


--echo '#--------------------FN_DYNVARS_005_03-------------------------#'
###############################################################################
# Change the value of myisam_repair_threads  to a valid value for GLOBAL Scope#
###############################################################################

SET @@global.myisam_repair_threads  = 1;
SELECT @@global.myisam_repair_threads ;

SET @@global.myisam_repair_threads  = 4294967295;
SELECT @@global.myisam_repair_threads ;

SET @@global.myisam_repair_threads  = 655354;
SELECT @@global.myisam_repair_threads ;

# Test cases for 64-bit machines
if($mach64)
{
  --disable_warnings
  SET @@global.myisam_repair_threads  = 18446744073709551615;
  --enable_warnings

  # Bug#11752618 - 64-BIT WINDOWS VARIABLES LIMITED TO DIFFERENT VALUES THAN 64-BIT LINUX/SO
  let $machine_name = `SELECT CONVERT(@@version_compile_os using latin1) IN ("Win64", "Windows")`;

  if($machine_name)
  {
    --replace_result 4294967295 18446744073709551615
  }
  SELECT @@global.myisam_repair_threads ;
}

--echo '#--------------------FN_DYNVARS_005_04-------------------------#'
###############################################################################
# Change the value of myisam_repair_threads  to a valid value for SESSION
# Scope 
###############################################################################
 
SET @@session.myisam_repair_threads  = 1;
SELECT @@session.myisam_repair_threads ;

SET @@session.myisam_repair_threads  = 4294967295;
SELECT @@session.myisam_repair_threads ;

SET @@session.myisam_repair_threads  = 655345;
SELECT @@session.myisam_repair_threads ;

# Test cases for 64-bit machines
if($mach64)
{
  --disable_warnings
  SET @@session.myisam_repair_threads  = 18446744073709551615;
  --enable_warnings

  # Bug#11752618 - 64-BIT WINDOWS VARIABLES LIMITED TO DIFFERENT VALUES THAN 64-BIT LINUX/SO
  if($machine_name)
  {
    --replace_result 4294967295 18446744073709551615
  }
  SELECT @@session.myisam_repair_threads ;
}

--echo '#------------------FN_DYNVARS_005_05-----------------------#'
##################################################################
# Change the value of myisam_repair_threads  to an invalid value #
##################################################################

SET @@global.myisam_repair_threads  = 0;
SELECT @@global.myisam_repair_threads ;

SET @@global.myisam_repair_threads  = -1024;
SELECT @@global.myisam_repair_threads ;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.myisam_repair_threads  = 65530.34;
SELECT @@global.myisam_repair_threads ;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.myisam_repair_threads  = test;
SELECT @@global.myisam_repair_threads ;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.myisam_repair_threads  = "test";
SELECT @@global.myisam_repair_threads ;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.myisam_repair_threads  = 'test';
SELECT @@global.myisam_repair_threads ;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.myisam_repair_threads  = ON;
SELECT @@global.myisam_repair_threads ;

if($mach32)
{
  SET @@global.myisam_repair_threads  = 4294967296;
  SELECT @@global.myisam_repair_threads ;

  SET @@global.myisam_repair_threads  = 42949672950;
  SELECT @@global.myisam_repair_threads ;
}

if($mach64)
{
  --error ER_WRONG_TYPE_FOR_VAR
  SET @@global.myisam_repair_threads = 18446744073709551616;
  SELECT @@global.myisam_repair_threads ;
}

SET @@session.myisam_repair_threads  = 0;
SELECT @@session.myisam_repair_threads ;

SET @@session.myisam_repair_threads  = -2;
SELECT @@session.myisam_repair_threads ;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.myisam_repair_threads  = 65530.34;
SELECT @@session.myisam_repair_threads ;


--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.myisam_repair_threads  = test;
SELECT @@session.myisam_repair_threads ;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.myisam_repair_threads  = "test";
SELECT @@session.myisam_repair_threads ;

if($mach32)
{
  SET @@session.myisam_repair_threads  = 4294967296;
  SELECT @@session.myisam_repair_threads ;

  SET @@session.myisam_repair_threads  = 42949672950;
  SELECT @@session.myisam_repair_threads ;
}

if($mach64)
{
  --error ER_WRONG_TYPE_FOR_VAR
  SET @@session.myisam_repair_threads = 18446744073709551616;
  SELECT @@session.myisam_repair_threads ;
}

--echo '#------------------FN_DYNVARS_005_06-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################

--disable_warnings
SELECT @@global.myisam_repair_threads  = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='myisam_repair_threads ';
--enable_warnings

--echo '#------------------FN_DYNVARS_005_07-----------------------#'
####################################################################
#  Check if the value in SESSION Table matches value in variable   #
####################################################################

--disable_warnings
SELECT @@session.myisam_repair_threads  = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='myisam_repair_threads ';
--enable_warnings


--echo '#------------------FN_DYNVARS_005_08-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.myisam_repair_threads  = TRUE;
SELECT @@global.myisam_repair_threads ;
SET @@global.myisam_repair_threads  = FALSE;
SELECT @@global.myisam_repair_threads ;



--echo '#---------------------FN_DYNVARS_001_09----------------------#'
###############################################################################
#  Check if accessing variable with and without GLOBAL point to same variable #
###############################################################################

SET @@global.myisam_repair_threads  = 10;
SELECT @@myisam_repair_threads  = @@global.myisam_repair_threads ;


--echo '#---------------------FN_DYNVARS_001_10----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to
#    same session variable  #
########################################################################################################

SET @@myisam_repair_threads  = 100;
SELECT @@myisam_repair_threads  = @@local.myisam_repair_threads ;
SELECT @@local.myisam_repair_threads  = @@session.myisam_repair_threads ;


--echo '#---------------------FN_DYNVARS_001_11----------------------#'
###################################################################################  
#   Check if myisam_repair_threads  can be accessed with and without @@ sign    #
###################################################################################

SET myisam_repair_threads  = 1;
SELECT @@myisam_repair_threads ;
--Error ER_UNKNOWN_TABLE

SELECT local.myisam_repair_threads ;

--Error ER_UNKNOWN_TABLE
SELECT session.myisam_repair_threads ;

--Error ER_BAD_FIELD_ERROR
SELECT myisam_repair_threads  = @@session.myisam_repair_threads ;


####################################
#     Restore initial value        #
####################################

SET @@global.myisam_repair_threads  = @start_global_value;
SELECT @@global.myisam_repair_threads ;
SET @@session.myisam_repair_threads  = @start_session_value;
SELECT @@session.myisam_repair_threads ;


#############################################################
#                 END OF myisam_repair_threads  TESTS     #
#############################################################


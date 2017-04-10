############### mysql-test\t\sort_buffer_size_basic.test ########################
#                                                                               #
# Variable Name: sort_buffer_size                                               #
# Scope: GLOBAL | SESSION                                                       #
# Access Type: Dynamic                                                          #
# Data Type: numeric                                                            #
# Default Value: 2097144                                                        #
# Range: 32768 - 4294967295/18446744073709551615                                #
#                                                                               #
#                                                                               #
# Creation Date: 2008-02-07                                                     #
# Author:  Salman                                                               #
#                                                                               #
# Description: Test Cases of Dynamic System Variable sort_buffer_size           #
#              that checks the behavior of this variable in the following ways  #
#              * Default Value                                                  #
#              * Valid & Invalid values                                         #
#              * Scope & Access method                                          #
#              * Data Integrity                                                 #
#                                                                               #
# Modified:    Horst Hunger, 2008-11-28                                         #
#              disabled warnings as they contain the values of this variable,   #
#              modified the check of the value only checking a range.           #
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

########################################################################


######################################################################
#           START OF sort_buffer_size TESTS                          #
######################################################################


#############################################################
#                 Save initial value                        #
#############################################################

--disable_warnings

SET @start_global_value = @@global.sort_buffer_size;
SET @start_session_value = @@session.sort_buffer_size;

--echo '#--------------------FN_DYNVARS_151_01-------------------------#'
######################################################################
#     Display the DEFAULT value of sort_buffer_size                  #
######################################################################

SET @@global.sort_buffer_size = 1000;
SET @@global.sort_buffer_size = DEFAULT;
eval
SELECT @@global.sort_buffer_size;

SET @@session.sort_buffer_size = 2000;
SET @@session.sort_buffer_size = DEFAULT;
eval 
SELECT @@session.sort_buffer_size;


--echo '#--------------------FN_DYNVARS_151_02-------------------------#'
######################################################################
#     Check the DEFAULT value of sort_buffer_size                    #
######################################################################

SET @@global.sort_buffer_size = DEFAULT;
eval 
SELECT @@global.sort_buffer_size;

SET @@session.sort_buffer_size = DEFAULT;
eval 
SELECT @@session.sort_buffer_size;


--echo '#--------------------FN_DYNVARS_151_03-------------------------#'
################################################################################
# Change the value of sort_buffer_size to a valid value for GLOBAL Scope       #
################################################################################

SET @@global.sort_buffer_size = 32768;
eval 
SELECT @@global.sort_buffer_size;
SET @@global.sort_buffer_size = 32777;
eval 
SELECT @@global.sort_buffer_size;
SET @@global.sort_buffer_size = 4294967295;
SELECT @@global.sort_buffer_size;
SET @@global.sort_buffer_size = 4294967294;
SELECT @@global.sort_buffer_size;

# Test cases for 64-bit machines
if($mach64)
{
  SET @@global.sort_buffer_size = 18446744073709551615;

  # Bug#11752618 - 64-BIT WINDOWS VARIABLES LIMITED TO DIFFERENT VALUES THAN 64-BIT LINUX/SO
  let $machine_name = `SELECT CONVERT(@@version_compile_os using latin1) IN ("Win64", "Windows")`;

  if($machine_name)
  {
    --replace_result 4294967295 18446744073709551615
  }
  SELECT @@global.sort_buffer_size;
}

--echo '#--------------------FN_DYNVARS_151_04-------------------------#'
###################################################################################
# Change the value of sort_buffer_size to a valid value for SESSION Scope         #
###################################################################################
 
SET @@session.sort_buffer_size = 32768;
eval 
SELECT @@session.sort_buffer_size;
SET @@session.sort_buffer_size = 32777;
eval 
SELECT @@session.sort_buffer_size;
SET @@session.sort_buffer_size = 4294967295;
SELECT @@session.sort_buffer_size;
SET @@session.sort_buffer_size = 4294967294;
SELECT @@session.sort_buffer_size;

# Test cases for 64-bit machines
if($mach64)
{
  SET @@session.sort_buffer_size = 18446744073709551615;

  # Bug#11752618 - 64-BIT WINDOWS VARIABLES LIMITED TO DIFFERENT VALUES THAN 64-BIT LINUX/SO
  if($machine_name)
  {
    --replace_result 4294967295 18446744073709551615
  }
  SELECT @@session.sort_buffer_size;
}

--echo '#------------------FN_DYNVARS_151_05-----------------------#'
##################################################################
# Change the value of sort_buffer_size to an invalid value       #
##################################################################

SET @@global.sort_buffer_size = 32767;
eval 
SELECT @@global.sort_buffer_size;
SET @@global.sort_buffer_size = -1024;
eval 
SELECT @@global.sort_buffer_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.sort_buffer_size = 65530.34;
SELECT @@global.sort_buffer_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.sort_buffer_size = test;
SELECT @@global.sort_buffer_size;

if($mach32)
{
  SET @@global.sort_buffer_size = 4294967296;
  SELECT @@global.sort_buffer_size;

  SET @@global.sort_buffer_size = 42949672950;
  SELECT @@global.sort_buffer_size;
}

if($mach64)
{
  --error ER_WRONG_TYPE_FOR_VAR
  SET @@global.sort_buffer_size = 18446744073709551616;
  SELECT @@global.sort_buffer_size;
}

SET @@session.sort_buffer_size = 32767;
eval 
SELECT @@session.sort_buffer_size;
SET @@session.sort_buffer_size = -2;
eval 
SELECT @@session.sort_buffer_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.sort_buffer_size = 65530.34;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.sort_buffer_size = test;

if($mach32)
{
  SET @@session.sort_buffer_size = 4294967296;
  SELECT @@session.sort_buffer_size;

  SET @@session.sort_buffer_size = 42949672950;
  SELECT @@session.sort_buffer_size;
}

if($mach64)
{
  --error ER_WRONG_TYPE_FOR_VAR
  SET @@session.sort_buffer_size = 18446744073709551616;
  SELECT @@session.sort_buffer_size;
}

--echo '#------------------FN_DYNVARS_151_06-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################


SELECT @@global.sort_buffer_size = VARIABLE_VALUE FROM 
       performance_schema.global_variables WHERE VARIABLE_NAME='sort_buffer_size';

--echo '#------------------FN_DYNVARS_151_07-----------------------#'
####################################################################
#  Check if the value in SESSION Table matches value in variable   #
####################################################################

SELECT @@session.sort_buffer_size = VARIABLE_VALUE FROM
performance_schema.session_variables WHERE VARIABLE_NAME='sort_buffer_size';


--echo '#------------------FN_DYNVARS_151_08-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.sort_buffer_size = TRUE;
eval 
SELECT @@global.sort_buffer_size;
SET @@global.sort_buffer_size = FALSE;
eval 
SELECT @@global.sort_buffer_size;

--echo '#---------------------FN_DYNVARS_151_09----------------------#'
#################################################################################### 
#  Check if accessing variable with and without GLOBAL point to same variable      #
#################################################################################### 

SET @@global.sort_buffer_size = 9000;
SELECT @@sort_buffer_size = @@global.sort_buffer_size;

--echo '#---------------------FN_DYNVARS_151_10----------------------#'
###################################################################################################
# Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable#
###################################################################################################

SET @@sort_buffer_size = 9000;
SELECT @@sort_buffer_size = @@local.sort_buffer_size;
SELECT @@local.sort_buffer_size = @@session.sort_buffer_size;


--echo '#---------------------FN_DYNVARS_151_11----------------------#'
###################################################################################  
#   Check if sort_buffer_size can be accessed with and without @@ sign            #
###################################################################################

SET sort_buffer_size = 9100;
eval 
SELECT @@sort_buffer_size;
--Error ER_UNKNOWN_TABLE
SELECT local.sort_buffer_size;
--Error ER_UNKNOWN_TABLE
SELECT session.sort_buffer_size;
--Error ER_BAD_FIELD_ERROR
SELECT sort_buffer_size = @@session.sort_buffer_size;


####################################
#     Restore initial value        #
####################################

SET @@global.sort_buffer_size = @start_global_value;
SET @@session.sort_buffer_size = @start_session_value;

--enable_warnings

#############################################################
#                 END OF sort_buffer_size TESTS             #
#############################################################


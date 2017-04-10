################# mysql-test\t\delayed_queue_size_basic.test ###################
#                                                                              #
# Variable Name: delayed_queue_size                                            #
# Scope: GLOBAL                                                                #
# Access Type: Dynamic                                                         #
# Data Type: Numeric                                                           #
# Default Value: 1000                                                          #
# Range: 1 - 4294967295/18446744073709551615                                   #
#                                                                              #
#                                                                              #
# Creation Date: 2008-02-07                                                    #
# Author:  Salman Rawala                                                       #
#                                                                              #
# Description: Test Cases of Dynamic System Variable "delayed_queue_size"      #
#              that checks behavior of this variable in the following ways     #
#              * Default Value                                                 #
#              * Valid & Invalid values                                        #
#              * Scope & Access method                                         #
#              * Data Integrity                                                #
#                                                                              #
# Modified:    Pavan Naik, 2015-11-24                                          #
#              Added boundary value test cases according to the type of the    #
#              server binary.                                                  #
#                                                                              #
# Reference: http://dev.mysql.com/doc/refman/5.1/en/                           #
#            server-system-variables.html#option_mysqld_delayed_queue_size     #
#                                                                              #
################################################################################

--source include/load_sysvars.inc

#################################################################
#           START OF delayed_queue_size TESTS                   #
#################################################################

#########################################################################
# Saving initial value of delayed_queue_size in a temporary variable    #
#########################################################################

SET @start_value = @@global.delayed_queue_size;
SELECT @start_value;

--echo '#--------------------FN_DYNVARS_026_01------------------------#'
######################################################################### 
#              Display the DEFAULT value of delayed_queue_size          #
######################################################################### 

SET @@global.delayed_queue_size = 100;
SET @@global.delayed_queue_size = DEFAULT;
SELECT @@global.delayed_queue_size;

--echo '#---------------------FN_DYNVARS_026_02-------------------------#'
############################################### 
#     Verify default value of variable        #
############################################### 

SET @@global.delayed_queue_size = @start_value;
SELECT @@global.delayed_queue_size = 1000;

--echo '#--------------------FN_DYNVARS_026_03------------------------#'
#########################################################################
#        Change the value of delayed_queue_size to a valid value        #
#########################################################################

SET @@global.delayed_queue_size = 1;
SELECT @@global.delayed_queue_size;
SET @@global.delayed_queue_size = 10000;
SELECT @@global.delayed_queue_size;
SET @@global.delayed_queue_size = 4294967295;
SELECT @@global.delayed_queue_size;
SET @@global.delayed_queue_size = 1;
SELECT @@global.delayed_queue_size;

# Test cases for 64-bit machines
if($mach64)
{
  --disable_warnings
  SET @@global.delayed_queue_size = 18446744073709551615;
  --enable_warnings

  # Bug#11752618 - 64-BIT WINDOWS VARIABLES LIMITED TO DIFFERENT VALUES THAN 64-BIT LINUX/SO
  let $machine_name = `SELECT CONVERT(@@version_compile_os using latin1) IN ("Win64", "Windows")`;

  if($machine_name)
  {
    --replace_result 4294967295 18446744073709551615
  }
  SELECT @@global.delayed_queue_size;
}

--echo '#--------------------FN_DYNVARS_026_04-------------------------#'
############################################################################
#         Change the value of delayed_queue_size to invalid value          #
############################################################################ 

SET @@global.delayed_queue_size = 0;
SELECT @@global.delayed_queue_size;
SET @@global.delayed_queue_size = -1024;
SELECT @@global.delayed_queue_size;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.delayed_queue_size = 429496729.5;
SELECT @@global.delayed_queue_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.delayed_queue_size = ON;
SELECT @@delayed_queue_size;

if($mach32)
{
  SET @@global.delayed_queue_size = 4294967296;
  SELECT @@global.delayed_queue_size;

  SET @@global.delayed_queue_size = 42949672950;
  SELECT @@global.delayed_queue_size;
}

if($mach64)
{
  --error ER_WRONG_TYPE_FOR_VAR
  SET @@global.delayed_queue_size = 18446744073709551616;
  SELECT @@global.delayed_queue_size;
}

--echo '#-------------------FN_DYNVARS_026_05----------------------------#'
############################################################################
#       Test if accessing session delayed_queue_size gives error           #
############################################################################

--Error ER_GLOBAL_VARIABLE
SET @@session.delayed_queue_size = 0;
--Error ER_INCORRECT_GLOBAL_LOCAL_VAR
SELECT @@session.delayed_queue_size;

--echo '#----------------------FN_DYNVARS_026_06------------------------#'
############################################################################## 
# Check if the value in GLOBAL & SESSION Tables matches values in variable   #
##############################################################################

--disable_warnings
SELECT VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='delayed_queue_size'
into @tmp;
--enable_warnings

SELECT @@global.delayed_queue_size = @tmp;

--disable_warnings
SELECT VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='delayed_queue_size'
into @tmp;
--enable_warnings

SELECT @@delayed_queue_size = @tmp;


--echo '#---------------------FN_DYNVARS_026_07----------------------#'
################################################################### 
#      Check if TRUE and FALSE values can be used on variable     #
################################################################### 

SET @@global.delayed_queue_size = TRUE;
SELECT @@global.delayed_queue_size;
SET @@global.delayed_queue_size = FALSE;
SELECT @@global.delayed_queue_size;

--echo '#---------------------FN_DYNVARS_026_08----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@global.delayed_queue_size = 1;
SELECT @@delayed_queue_size = @@global.delayed_queue_size;


--echo '#---------------------FN_DYNVARS_026_09----------------------#'
###########################################################################
#   Check if delayed_queue_size can be accessed with and without @@ sign  #
###########################################################################

--Error ER_GLOBAL_VARIABLE
SET delayed_queue_size = 1;
SELECT @@delayed_queue_size;
--Error ER_PARSE_ERROR
SET local.delayed_queue_size = 1;
--Error ER_UNKNOWN_TABLE
SELECT local.delayed_queue_size;
--Error ER_PARSE_ERROR
SET global.delayed_queue_size = 1;
--Error ER_UNKNOWN_TABLE
SELECT global.delayed_queue_size;
--Error ER_BAD_FIELD_ERROR
SELECT delayed_queue_size = @@session.delayed_queue_size;


##############################  
#   Restore initial value    #
##############################

SET @@global.delayed_queue_size = @start_value;
SELECT @@global.delayed_queue_size;


###########################################################
#              END OF delayed_queue_size TESTS            #
###########################################################


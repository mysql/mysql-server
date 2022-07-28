################# mysql-test\t\binlog_cache_size_basic.test ###################
#                                                                             #
# Variable Name: binlog_cache_size                                            #
# Scope: GLOBAL                                                               #
# Access Type: Dynamic                                                        #
# Data Type: Numeric                                                          #
# Default Value: 32768                                                        #
# Range: 4096 - 4294967295/18446744073709551615                               #
#                                                                             #
#                                                                             #
# Creation Date: 2008-04-28                                                   #
# Author:  Salman Rawala/Horst Hunger                                         #
#                                                                             #
# Description: Test Cases of Dynamic System Variable "binlog_cache_size"      #
#              that checks behavior of this variable in the following ways    #
#              * Default Value                                                #
#              * Valid & Invalid values                                       #
#              * Scope & Access method                                        #
#              * Data Integrity                          .                    #
#                                                                             #
# Modified:    Pavan Naik, 2015-11-24                                         #
#              Added boundary value test cases according to the type of the   #
#              server binary.                                                 #
#                                                                             #
# Reference: http://dev.mysql.com/doc/refman/5.1/en/                          #
#            server-system-variables.html#option_mysqld_binlog_cache_size     #
#                                                                             #
###############################################################################

#################################################################
#           START OF binlog_cache_size TESTS                    #
#################################################################

#########################################################################
# Saving initial value of binlog_cache_size in a temporary variable     #
#########################################################################

SET @start_value = @@global.binlog_cache_size;
SELECT @start_value;

--echo '#--------------------FN_DYNVARS_006_01------------------------#'
######################################################################### 
#              Display the DEFAULT value of binlog_cache_size           #
######################################################################### 

SET @@global.binlog_cache_size = 100;
SET @@global.binlog_cache_size = DEFAULT;
SELECT @@global.binlog_cache_size;


--echo '#---------------------FN_DYNVARS_006_02-------------------------#'
############################################### 
#     Verify default value of variable        #
############################################### 

SET @@global.binlog_cache_size = @start_value;
SELECT @@global.binlog_cache_size = 32768;


--echo '#--------------------FN_DYNVARS_006_03------------------------#'
#########################################################################
#        Change the value of binlog_cache_size to a valid value         #
#########################################################################

SET @@global.binlog_cache_size = 4096;
SELECT @@global.binlog_cache_size;
SET @@global.binlog_cache_size = 4294967295;
SELECT @@global.binlog_cache_size;
SET @@global.binlog_cache_size = 10000;
SELECT @@global.binlog_cache_size;
SET @@global.binlog_cache_size = 21221204;
SELECT @@global.binlog_cache_size;
echo 'Bug: Invalid values are coming in variable on assigning valid values';

# Test cases for 64-bit machines
if($mach64)
{
  --disable_warnings
  SET @@global.binlog_cache_size = 18446744073709547520;
  --enable_warnings

  # Bug#11752618 - 64-BIT WINDOWS VARIABLES LIMITED TO DIFFERENT VALUES THAN 64-BIT LINUX/SO
  let $machine_name = `SELECT CONVERT(@@version_compile_os using latin1) IN ("Win64", "Windows")`;

  if($machine_name)
  {
    --replace_result 4294963200 18446744073709547520
  }
  SELECT @@global.binlog_cache_size;
}

--echo '#--------------------FN_DYNVARS_006_04-------------------------#'
############################################################################
#         Change the value of binlog_cache_size to invalid value           #
############################################################################ 

SET @@global.binlog_cache_size = 4095;
SELECT @@global.binlog_cache_size;
SET @@global.binlog_cache_size = 1024;
SELECT @@global.binlog_cache_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.binlog_cache_size = 10000.01;
SET @@global.binlog_cache_size = 0;
SELECT @@global.binlog_cache_size;
SET @@global.binlog_cache_size = -1024;
SELECT @@global.binlog_cache_size;
echo 'Bug: Errors are not coming on assigning invalid values to variable';

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.binlog_cache_size = ON;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.binlog_cache_size = 'test';

if($mach32)
{
  SET @@global.binlog_cache_size = 4294967296;
  SELECT @@global.binlog_cache_size;

  SET @@global.binlog_cache_size = 42949672950;
  SELECT @@global.binlog_cache_size;
}

if($mach64)
{
  SET @@global.binlog_cache_size = 18446744073709547521;

  # Bug#11752618 - 64-BIT WINDOWS VARIABLES LIMITED TO DIFFERENT VALUES THAN 64-BIT LINUX/SO
  if($machine_name)
  {
    --replace_result 4294963200 18446744073709547520
  }
  SELECT @@global.binlog_cache_size;
}

--echo '#-------------------FN_DYNVARS_006_05----------------------------#'
############################################################################
#       Test if accessing session binlog_cache_size gives error            #
############################################################################

--Error ER_GLOBAL_VARIABLE
SET @@session.binlog_cache_size = 0;


--echo '#----------------------FN_DYNVARS_006_06------------------------#'
############################################################################## 
# Check if the value in GLOBAL Tables matches values in variable             #
##############################################################################

SET @@global.binlog_cache_size = 4294967295;

--disable_warnings
SELECT @@global.binlog_cache_size = VARIABLE_VALUE FROM performance_schema.global_variables WHERE VARIABLE_NAME='binlog_cache_size';
--enable_warnings

--echo '#---------------------FN_DYNVARS_006_07----------------------#'
################################################################### 
#      Check if TRUE and FALSE values can be used on variable     #
################################################################### 

SET @@global.binlog_cache_size = TRUE;
SELECT @@global.binlog_cache_size;
SET @@global.binlog_cache_size = FALSE;
SELECT @@global.binlog_cache_size;
echo 'Bug: Errors are not coming on assigning TRUE/FALSE to variable';

--echo '#---------------------FN_DYNVARS_006_08----------------------#'
###############################################################################
#    Check if accessing variable without SCOPE points to same global variable #
###############################################################################

SET @@global.binlog_cache_size = 1;
SELECT @@binlog_cache_size = @@global.binlog_cache_size;

--echo '#---------------------FN_DYNVARS_006_09----------------------#'
###########################################################################
#   Check if binlog_cache_size can be accessed with and without @@ sign   #
###########################################################################

--Error ER_GLOBAL_VARIABLE
SET binlog_cache_size = 1;
--Error ER_PARSE_ERROR
SET global.binlog_cache_size = 1;
--Error ER_UNKNOWN_TABLE
SELECT global.binlog_cache_size;
--Error ER_BAD_FIELD_ERROR
SELECT binlog_cache_size;
--Error ER_INCORRECT_GLOBAL_LOCAL_VAR
SELECT @@session.binlog_cache_size;


##############################  
#   Restore initial value    #
##############################

SET @@global.binlog_cache_size = @start_value;
SELECT @@global.binlog_cache_size;


###########################################################
#              END OF binlog_cache_size TESTS             #
###########################################################

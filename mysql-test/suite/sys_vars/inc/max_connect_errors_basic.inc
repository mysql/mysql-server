############## mysql-test\t\max_connect_errors_basic.test ###############
#                                                                             #
# Variable Name: max_connect_errors                                           #
# Scope: GLOBAL                                                               #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value:  100                                                         #
# Range:    1-4294967295                                                      #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable max_connect_errors       #
#              that checks the behavior of this variable in the following ways#
#              * Default Value                                                #
#              * Valid & Invalid values                                       #
#              * Scope & Access method                                        #
#              * Data Integrity                                               #
#                                                                             #
# Reference: http://dev.mysql.com/doc/refman/5.1/en/                          #
#  server-system-variables.html                                               #
#                                                                             #
###############################################################################

--source include/load_sysvars.inc

##################################################################
#              START OF max_connect_errors TESTS                 #
##################################################################


######################################################################
# Saving initial value of max_connect_errors in a temporary variable #
######################################################################

SET @start_value = @@global.max_connect_errors;
SELECT @start_value;


--echo '#--------------------FN_DYNVARS_073_01------------------------#'
##################################################################
#           Display the DEFAULT value of max_connect_errors         #
##################################################################

SET @@global.max_connect_errors = 5000;
SET @@global.max_connect_errors = DEFAULT;
SELECT @@global.max_connect_errors;

--echo '#---------------------FN_DYNVARS_073_02-------------------------#'
############################################### 
#     Verify default value of variable        #
############################################### 

SET @@global.max_connect_errors = @start_value;
SELECT @@global.max_connect_errors = 100;

--echo '#--------------------FN_DYNVARS_073_03------------------------#'
##################################################################
#    Change the value of max_connect_errors to a valid value        #
##################################################################

SET @@global.max_connect_errors = 4096;
SELECT @@global.max_connect_errors;
SET @@global.max_connect_errors = 4294967294;
SELECT @@global.max_connect_errors;
SET @@global.max_connect_errors = 4294967295;
SELECT @@global.max_connect_errors;
SET @@global.max_connect_errors = 1;
SELECT @@global.max_connect_errors;
SET @@global.max_connect_errors = 2;
SELECT @@global.max_connect_errors;


--echo '#--------------------FN_DYNVARS_073_04-------------------------#'
#####################################################################
#      Change the value of max_connect_errors to invalid value         #
#####################################################################

SET @@global.max_connect_errors = -1;
SELECT @@global.max_connect_errors;
SET @@global.max_connect_errors = 100000000000;
SELECT @@global.max_connect_errors;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.max_connect_errors = 10000.01;
SELECT @@global.max_connect_errors;
SET @@global.max_connect_errors = -1024;
SELECT @@global.max_connect_errors;
SET @@global.max_connect_errors = 0;
SELECT @@global.max_connect_errors;
SET @@global.max_connect_errors = 4294967296;
SELECT @@global.max_connect_errors;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.max_connect_errors = ON;
SELECT @@global.max_connect_errors;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.max_connect_errors = 'test';
SELECT @@global.max_connect_errors;


--echo '#-------------------FN_DYNVARS_073_05----------------------------#'
##################################################################### 
#       Test if accessing session max_connect_errors gives error       #
#####################################################################

--Error ER_GLOBAL_VARIABLE
SET @@session.max_connect_errors = 4096;
--Error ER_INCORRECT_GLOBAL_LOCAL_VAR
SELECT @@session.max_connect_errors;


--echo '#----------------------FN_DYNVARS_073_06------------------------#'
############################################################################## 
# Check if the value in GLOBAL & SESSION Tables matches values in variable   #
##############################################################################

--disable_warnings
SELECT @@global.max_connect_errors = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='max_connect_errors';

SELECT @@max_connect_errors = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='max_connect_errors';
--enable_warnings


--echo '#---------------------FN_DYNVARS_073_07----------------------#'
################################################################### 
#      Check if TRUE and FALSE values can be used on variable     #
################################################################### 

SET @@global.max_connect_errors = TRUE;
SELECT @@global.max_connect_errors;
SET @@global.max_connect_errors = FALSE;
SELECT @@global.max_connect_errors;


--echo '#---------------------FN_DYNVARS_073_08----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@global.max_connect_errors = 5000;
SELECT @@max_connect_errors = @@global.max_connect_errors;


--echo '#---------------------FN_DYNVARS_073_09----------------------#'
##########################################################################
#   Check if max_connect_errors can be accessed with and without @@ sign    #
##########################################################################

--Error ER_GLOBAL_VARIABLE
SET max_connect_errors = 6000;
SELECT @@max_connect_errors;
--Error ER_PARSE_ERROR
SET local.max_connect_errors = 7000;
--Error ER_UNKNOWN_TABLE
SELECT local.max_connect_errors;
--Error ER_PARSE_ERROR
SET global.max_connect_errors = 8000;
--Error ER_UNKNOWN_TABLE
SELECT global.max_connect_errors;
--Error ER_BAD_FIELD_ERROR
SELECT max_connect_errors = @@session.max_connect_errors;


##############################  
#   Restore initial value    #
##############################

SET @@global.max_connect_errors = @start_value;
SELECT @@global.max_connect_errors;


##################################################################
#              END OF max_connect_errors TESTS                      #
##################################################################


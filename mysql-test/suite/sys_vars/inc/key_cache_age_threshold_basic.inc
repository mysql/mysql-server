############## mysql-test\t\key_cache_age_threshold_basic.test ###############
#                                                                             #
# Variable Name: key_cache_age_threshold                                      #
# Scope: GLOBAL                                                               #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value: 300                                                          #
# Range: 100-4294967295                                                       #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable key_cache_age_threshold  #
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

######################################################################## 
#            START OF key_cache_age_threshold TESTS                    #
######################################################################## 


############################################################################# 
#  Saving initial value of key_cache_age_threshold in a temporary variable  #
############################################################################# 

SET @start_value = @@global.key_cache_age_threshold;
SELECT @start_value;


--echo '#--------------------FN_DYNVARS_056_01------------------------#'
################################################################################ 
#              Display the DEFAULT value of key_cache_age_threshold            #
################################################################################ 

SET @@global.key_cache_age_threshold = DEFAULT;
SELECT @@global.key_cache_age_threshold;


--echo '#---------------------FN_DYNVARS_056_02-------------------------#'
############################################### 
#     Verify default value of variable        #
############################################### 

SET @@global.key_cache_age_threshold = @start_value;
SELECT @@global.key_cache_age_threshold = 300;


--echo '#--------------------FN_DYNVARS_056_03------------------------#'
############################################################################### 
#        Change the value of key_cache_age_threshold to a valid value         #
############################################################################### 

SET @@global.key_cache_age_threshold = 100;
SELECT @@global.key_cache_age_threshold;
SET @@global.key_cache_age_threshold = 4294967295;
SELECT @@global.key_cache_age_threshold;
SET @@global.key_cache_age_threshold = 1800;
SELECT @@global.key_cache_age_threshold;
SET @@global.key_cache_age_threshold = 65535;
SELECT @@global.key_cache_age_threshold;

--echo '#--------------------FN_DYNVARS_056_04-------------------------#'
########################################################################### 
#         Change the value of key_cache_age_threshold to invalid value    #
########################################################################### 

SET @@global.key_cache_age_threshold = -1;
SELECT @@global.key_cache_age_threshold;
SET @@global.key_cache_age_threshold = 42949672951;
SELECT @@global.key_cache_age_threshold;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.key_cache_age_threshold = 10000.01;
SELECT @@global.key_cache_age_threshold;
SET @@global.key_cache_age_threshold = -1024;
SELECT @@global.key_cache_age_threshold;
SET @@global.key_cache_age_threshold = 99;
SELECT @@global.key_cache_age_threshold;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.key_cache_age_threshold = ON;
SELECT @@global.key_cache_age_threshold;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.key_cache_age_threshold = 'test';
SELECT @@global.key_cache_age_threshold;


--echo '#-------------------FN_DYNVARS_056_05----------------------------#'
########################################################################### 
#       Test if accessing session key_cache_age_threshold gives error     #
########################################################################### 

--Error ER_GLOBAL_VARIABLE
SET @@session.key_cache_age_threshold = 0;
--Error ER_INCORRECT_GLOBAL_LOCAL_VAR
SELECT @@session.key_cache_age_threshold;


--echo '#----------------------FN_DYNVARS_056_06------------------------#'
############################################################################## 
# Check if the value in GLOBAL & SESSION Tables matches values in variable   #
##############################################################################

--disable_warnings
SELECT @@global.key_cache_age_threshold = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='key_cache_age_threshold';

SELECT @@key_cache_age_threshold = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='key_cache_age_threshold';
--enable_warnings


--echo '#---------------------FN_DYNVARS_056_07----------------------#'
################################################################### 
#      Check if TRUE and FALSE values can be used on variable     #
################################################################### 

SET @@global.key_cache_age_threshold = TRUE;
SELECT @@global.key_cache_age_threshold;
SET @@global.key_cache_age_threshold = FALSE;
SELECT @@global.key_cache_age_threshold;


--echo '#---------------------FN_DYNVARS_056_08----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@global.key_cache_age_threshold = 101;
SELECT @@key_cache_age_threshold = @@global.key_cache_age_threshold;


--echo '#---------------------FN_DYNVARS_056_09----------------------#'
########################################################################## ####### 
#   Check if key_cache_age_threshold can be accessed with and without @@ sign    #
##################################################################################

--Error ER_GLOBAL_VARIABLE
SET key_cache_age_threshold = 8000;
SELECT @@key_cache_age_threshold;
--Error ER_PARSE_ERROR
SET local.key_cache_age_threshold = 10;
--Error ER_UNKNOWN_TABLE
SELECT local.key_cache_age_threshold;
--Error ER_PARSE_ERROR
SET global.key_cache_age_threshold = 10;
--Error ER_UNKNOWN_TABLE
SELECT global.key_cache_age_threshold;
--Error ER_BAD_FIELD_ERROR
SELECT key_cache_age_threshold = @@session.key_cache_age_threshold;


##############################  
#   Restore initial value    #
##############################

SET @@global.key_cache_age_threshold = @start_value;
SELECT @@global.key_cache_age_threshold;


######################################################################## 
#              END OF key_cache_age_threshold TESTS                    #
######################################################################## 

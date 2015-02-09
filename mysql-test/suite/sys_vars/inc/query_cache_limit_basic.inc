############## mysql-test\t\query_cache_limit_basic.test ###############
#                                                                             #
# Variable Name: query_cache_limit                                            #
# Scope: GLOBAL                                                               #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value:  1048576                                                     #
# Min value:     0                                                            #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable query_cache_limit        #
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

--source include/have_query_cache.inc
--source include/load_sysvars.inc

######################################################################## 
#                    START OF query_cache_limit TESTS                  #
######################################################################## 


######################################################################## 
# Saving initial value of query_cache_limit in a temporary variable    #
######################################################################## 

SET @start_value = @@global.query_cache_limit;
SELECT @start_value;


--echo '#--------------------FN_DYNVARS_131_01------------------------#'
######################################################################## 
#              Display the DEFAULT value of query_cache_limit          #
######################################################################## 

SET @@global.query_cache_limit = 99;
SET @@global.query_cache_limit = DEFAULT;
SELECT @@global.query_cache_limit;


--echo '#---------------------FN_DYNVARS_131_02-------------------------#'
############################################### 
#     Verify default value of variable        #
############################################### 

SET @@global.query_cache_limit = @start_value;
SELECT @@global.query_cache_limit = 1048576;


--echo '#--------------------FN_DYNVARS_131_03------------------------#'
######################################################################## 
#        Change the value of query_cache_limit to a valid value         #
######################################################################## 

SET @@global.query_cache_limit = 0;
SELECT @@global.query_cache_limit;
SET @@global.query_cache_limit = 1;
SELECT @@global.query_cache_limit;
SET @@global.query_cache_limit = 1048576;
SELECT @@global.query_cache_limit;
SET @@global.query_cache_limit = 1048575;
SELECT @@global.query_cache_limit;


--echo '#--------------------FN_DYNVARS_131_04-------------------------#'
########################################################################### 
#         Change the value of query_cache_limit to invalid value           #
########################################################################### 

SET @@global.query_cache_limit = -1;
SELECT @@global.query_cache_limit;
SET @@global.query_cache_limit = 4294967296;
SELECT @@global.query_cache_limit;
SET @@global.query_cache_limit = 10240022115;
SELECT @@global.query_cache_limit;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.query_cache_limit = 10000.01;
SELECT @@global.query_cache_limit;
SET @@global.query_cache_limit = -1024;
SELECT @@global.query_cache_limit;
SET @@global.query_cache_limit = 42949672950;
SELECT @@global.query_cache_limit;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.query_cache_limit = ON;
SELECT @@global.query_cache_limit;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.query_cache_limit = 'test';
SELECT @@global.query_cache_limit;


--echo '#-------------------FN_DYNVARS_131_05----------------------------#'
########################################################################### 
#       Test if accessing session query_cache_limit gives error            #
########################################################################### 

--Error ER_GLOBAL_VARIABLE
SET @@session.query_cache_limit = 0;
SELECT @@query_cache_limit;


--echo '#----------------------FN_DYNVARS_131_06------------------------#'
############################################################################## 
# Check if the value in GLOBAL & SESSION Tables matches values in variable   #
##############################################################################

--disable_warnings
SELECT @@global.query_cache_limit = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='query_cache_limit';

SELECT @@query_cache_limit = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='query_cache_limit';
--enable_warnings


--echo '#---------------------FN_DYNVARS_131_07----------------------#'
################################################################### 
#      Check if TRUE and FALSE values can be used on variable     #
################################################################### 

SET @@global.query_cache_limit = TRUE;
SELECT @@global.query_cache_limit;
SET @@global.query_cache_limit = FALSE;
SELECT @@global.query_cache_limit;


--echo '#---------------------FN_DYNVARS_131_08----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@global.query_cache_limit = 1;
SELECT @@query_cache_limit = @@global.query_cache_limit;


--echo '#---------------------FN_DYNVARS_131_09----------------------#'
##########################################################################  
#   Check if query_cache_limit can be accessed with and without @@ sign   #
##########################################################################

--Error ER_GLOBAL_VARIABLE
SET query_cache_limit = 1;
SELECT @@query_cache_limit;
--Error ER_PARSE_ERROR
SET local.query_cache_limit = 1;
--Error ER_UNKNOWN_TABLE
SELECT local.query_cache_limit;
--Error ER_PARSE_ERROR
SET global.query_cache_limit = 1;
--Error ER_UNKNOWN_TABLE
SELECT global.query_cache_limit;
--Error ER_BAD_FIELD_ERROR
SELECT query_cache_limit = @@session.query_cache_limit;


##############################  
#   Restore initial value    #
##############################

SET @@global.query_cache_limit = @start_value;
SELECT @@global.query_cache_limit;


######################################################################## 
#              END OF query_cache_limit TESTS                           #
######################################################################## 

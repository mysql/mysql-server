############## mysql-test\t\query_cache_size_basic.test ###############
#                                                                             #
# Variable Name: query_cache_size                                             #
# Scope: GLOBAL                                                               #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value:  1048576 (1M)                                                #
# Range:     -                                                                #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable query_cache_size         #
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
#             START OF query_cache_size TESTS                  #
######################################################################## 


############################################################################### 
# Saving initial value of query_cache_size in a temporary variable    #
############################################################################### 

SET @start_value = @@global.query_cache_size;

--echo '#--------------------FN_DYNVARS_133_01------------------------#'
############################################################################### 
#              Display the DEFAULT value of query_cache_size          #
############################################################################### 

SET @@global.query_cache_size = 99;
SET @@global.query_cache_size = DEFAULT;
SELECT @@global.query_cache_size;

--echo '#---------------------FN_DYNVARS_133_02-------------------------#'
############################################### 
#     Verify default value of variable        #
############################################### 

SET @@global.query_cache_size = @start_value;
SELECT @@global.query_cache_size = @start_value; 

--echo '#--------------------FN_DYNVARS_133_03------------------------#'
################################################################################ 
#        Change the value of query_cache_size to a valid value         #
################################################################################ 

SET @@global.query_cache_size = 0;
SELECT @@global.query_cache_size;
SET @@global.query_cache_size = 1;
SELECT @@global.query_cache_size;

SET @@global.query_cache_size = 512;
SELECT @@global.query_cache_size;
SET @@global.query_cache_size = 1024;
SELECT @@global.query_cache_size;
SET @@global.query_cache_size = 1048576;
SELECT @@global.query_cache_size;
SET @@global.query_cache_size = 1048575;
SELECT @@global.query_cache_size;

--echo '#--------------------FN_DYNVARS_133_04-------------------------#'
################################################################################### 
#         Change the value of query_cache_size to invalid value           #
################################################################################### 

SET @@global.query_cache_size = -1;
SELECT @@global.query_cache_size;

#
# Bug 11748572 - 36747: ALLOCATING A LARGE QUERY CACHE IS NOT DETERMINISTIC
#
# SET @@global.query_cache_size = 4294967296;
# SELECT @@global.query_cache_size;

SET @@global.query_cache_size = 511;
SELECT @@global.query_cache_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.query_cache_size = 10000.01;
SELECT @@global.query_cache_size;
SET @@global.query_cache_size = -1024;
SELECT @@global.query_cache_size;
SET @@global.query_cache_size = 42949672950;
SELECT @@global.query_cache_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.query_cache_size = ON;
SELECT @@global.query_cache_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.query_cache_size = 'test';
SELECT @@global.query_cache_size;

--echo '#-------------------FN_DYNVARS_133_05----------------------------#'
################################################################################### 
#       Test if accessing session query_cache_size gives error            #
################################################################################### 

--Error ER_GLOBAL_VARIABLE
SET @@session.query_cache_size = 0;
SELECT @@query_cache_size;

--echo '#----------------------FN_DYNVARS_133_06------------------------#'
############################################################################## 
# Check if the value in GLOBAL & SESSION Tables matches values in variable   #
##############################################################################

SELECT @@global.query_cache_size = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='query_cache_size';

SELECT @@query_cache_size = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES
WHERE VARIABLE_NAME='query_cache_size';

--echo '#---------------------FN_DYNVARS_133_07----------------------#'
################################################################### 
#      Check if TRUE and FALSE values can be used on variable     #
################################################################### 

SET @@global.query_cache_size = TRUE;
SELECT @@global.query_cache_size;
SET @@global.query_cache_size = FALSE;
SELECT @@global.query_cache_size;

--echo '#---------------------FN_DYNVARS_133_08----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@global.query_cache_size = 1;
SELECT @@query_cache_size = @@global.query_cache_size;

--echo '#---------------------FN_DYNVARS_133_09----------------------#'
##################################################################################  
#   Check if query_cache_size can be accessed with and without @@ sign   #
##################################################################################

--Error ER_GLOBAL_VARIABLE
SET query_cache_size = 1;
SELECT @@query_cache_size;
--Error ER_PARSE_ERROR
SET local.query_cache_size = 1;
--Error ER_UNKNOWN_TABLE
SELECT local.query_cache_size;
--Error ER_PARSE_ERROR
SET global.query_cache_size = 1;
--Error ER_UNKNOWN_TABLE
SELECT global.query_cache_size;
--Error ER_BAD_FIELD_ERROR
SELECT query_cache_size = @@session.query_cache_size;

##############################  
#   Restore initial value    #
##############################

SET @@global.query_cache_size = @start_value;

######################################################################## 
#              END OF query_cache_size TESTS                   #
######################################################################## 


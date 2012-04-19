############## mysql-test\t\query_cache_min_res_unit_basic.test ###############
#                                                                             #
# Variable Name: query_cache_min_res_unit                                     #
# Scope: GLOBAL                                                               #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value:1024                                                          #
# Min Value:0                                                                 #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman Rawala                                                      #
#                                                                             #
# Description: Test Cases of Dynamic System Variable query_cache_min_res_unit #
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


######################################################################## 
#             START OF query_cache_min_res_unit TESTS                  #
######################################################################## 


############################################################################### 
# Saving initial value of query_cache_min_res_unit in a temporary variable    #
############################################################################### 

SET @start_value = @@global.query_cache_min_res_unit;
SELECT @start_value;


--echo '#--------------------FN_DYNVARS_132_01------------------------#'
############################################################################### 
#              Display the DEFAULT value of query_cache_min_res_unit          #
############################################################################### 

SET @@global.query_cache_min_res_unit = 99;
SET @@global.query_cache_min_res_unit = DEFAULT;
SELECT @@global.query_cache_min_res_unit;


--echo '#---------------------FN_DYNVARS_132_02-------------------------#'
############################################### 
#     Verify default value of variable        #
############################################### 

SET @@global.query_cache_min_res_unit = @start_value;
SELECT @@global.query_cache_min_res_unit = 4096;


--echo '#--------------------FN_DYNVARS_132_03------------------------#'
################################################################################ 
#        Change the value of query_cache_min_res_unit to a valid value         #
################################################################################ 

SET @@global.query_cache_min_res_unit = 0;
SELECT @@global.query_cache_min_res_unit;
SET @@global.query_cache_min_res_unit = 1;
SELECT @@global.query_cache_min_res_unit;
SET @@global.query_cache_min_res_unit = 512;
SELECT @@global.query_cache_min_res_unit;
SET @@global.query_cache_min_res_unit = 513;
SELECT @@global.query_cache_min_res_unit;
SET @@global.query_cache_min_res_unit = 1048576;
SELECT @@global.query_cache_min_res_unit;
SET @@global.query_cache_min_res_unit = 1048575;
SELECT @@global.query_cache_min_res_unit;


--echo '#--------------------FN_DYNVARS_132_04-------------------------#'
################################################################################### 
#         Change the value of query_cache_min_res_unit to invalid value           #
################################################################################### 

SET @@global.query_cache_min_res_unit = -1;
SELECT @@global.query_cache_min_res_unit;
SET @@global.query_cache_min_res_unit = 4294967296;
SELECT @@global.query_cache_min_res_unit;
SET @@global.query_cache_min_res_unit = 511;
SELECT @@global.query_cache_min_res_unit;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.query_cache_min_res_unit = 10000.01;
SELECT @@global.query_cache_min_res_unit;
SET @@global.query_cache_min_res_unit = -1024;
SELECT @@global.query_cache_min_res_unit;
SET @@global.query_cache_min_res_unit = 42949672950;
SELECT @@global.query_cache_min_res_unit;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.query_cache_min_res_unit = ON;
SELECT @@global.query_cache_min_res_unit;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.query_cache_min_res_unit = 'test';
SELECT @@global.query_cache_min_res_unit;


--echo '#-------------------FN_DYNVARS_132_05----------------------------#'
################################################################################### 
#       Test if accessing session query_cache_min_res_unit gives error            #
################################################################################### 

--Error ER_GLOBAL_VARIABLE
SET @@session.query_cache_min_res_unit = 0;
SELECT @@query_cache_min_res_unit;


--echo '#----------------------FN_DYNVARS_132_06------------------------#'
############################################################################## 
# Check if the value in GLOBAL & SESSION Tables matches values in variable   #
##############################################################################

SELECT @@global.query_cache_min_res_unit = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='query_cache_min_res_unit';

SELECT @@query_cache_min_res_unit = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='query_cache_min_res_unit';


--echo '#---------------------FN_DYNVARS_132_07----------------------#'
################################################################### 
#      Check if TRUE and FALSE values can be used on variable     #
################################################################### 

SET @@global.query_cache_min_res_unit = TRUE;
SELECT @@global.query_cache_min_res_unit;
SET @@global.query_cache_min_res_unit = FALSE;
SELECT @@global.query_cache_min_res_unit;


--echo '#---------------------FN_DYNVARS_132_08----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@global.query_cache_min_res_unit = 1;
SELECT @@query_cache_min_res_unit = @@global.query_cache_min_res_unit;


--echo '#---------------------FN_DYNVARS_132_09----------------------#'
##################################################################################  
#   Check if query_cache_min_res_unit can be accessed with and without @@ sign   #
##################################################################################

--Error ER_GLOBAL_VARIABLE
SET query_cache_min_res_unit = 1;
SELECT @@query_cache_min_res_unit;
--Error ER_PARSE_ERROR
SET local.query_cache_min_res_unit = 1;
--Error ER_UNKNOWN_TABLE
SELECT local.query_cache_min_res_unit;
--Error ER_PARSE_ERROR
SET global.query_cache_min_res_unit = 1;
--Error ER_UNKNOWN_TABLE
SELECT global.query_cache_min_res_unit;
--Error ER_BAD_FIELD_ERROR
SELECT query_cache_min_res_unit = @@session.query_cache_min_res_unit;


##############################  
#   Restore initial value    #
##############################

SET @@global.query_cache_min_res_unit = @start_value;
SELECT @@global.query_cache_min_res_unit;


######################################################################## 
#              END OF query_cache_min_res_unit TESTS                   #
######################################################################## 

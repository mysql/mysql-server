############## mysql-test\t\key_buffer_size_basic.test ########################
#                                                                             #
# Variable Name: key_buffer_size                                              #
# Scope: GLOBAL                                                               #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value: 131072                                                       #
# Range:8-4294967295                                                          #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable key_buffer_size          #
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
# Changes:                                                             #
# 2008-03-06 hhunger Got "lost connections with 5.1.24                 #
########################################################################

--source include/load_sysvars.inc

######################################################################## 
#                    START OF key_buffer_size TESTS                    #
######################################################################## 

--disable_warnings
######################################################################## 
# Saving initial value of key_buffer_size in a temporary variable      #
######################################################################## 

SET @start_value = @@global.key_buffer_size;
SELECT @start_value;


--echo '#--------------------FN_DYNVARS_055_01------------------------#'
######################################################################## 
#              Display the DEFAULT value of key_buffer_size            #
######################################################################## 

SET @@global.key_buffer_size = 99;
--Error ER_NO_DEFAULT
SET @@global.key_buffer_size = DEFAULT;
--echo 'Bug# 34878: This variable has default value according to documentation';
SELECT @@global.key_buffer_size = @min_key_buffer_size;


--echo '#---------------------FN_DYNVARS_055_02-------------------------#'
############################################### 
#     Verify default value of variable        #
############################################### 

SET @@global.key_buffer_size = @start_value;
SELECT @@global.key_buffer_size = @start_value;

--echo '#--------------------FN_DYNVARS_055_03------------------------#'
######################################################################## 
#        Change the value of key_buffer_size to a valid value          #
######################################################################## 

SET @@global.key_buffer_size = @min_key_buffer_size;
SELECT @@global.key_buffer_size= @min_key_buffer_size;
--disable_warnings
#Due to "lost connection"
#SET @@global.key_buffer_size = 4294967295;
--enable_warnings
#SELECT @@global.key_buffer_size;
SET @@global.key_buffer_size = 1800;
SELECT @@global.key_buffer_size = @min_key_buffer_size;
SET @@global.key_buffer_size = 65535;
SELECT @@global.key_buffer_size;
echo 'Bug # 34837: Errors are not coming on assigning invalid values to variable';


--echo '#--------------------FN_DYNVARS_055_04-------------------------#'
########################################################################### 
#         Change the value of key_buffer_size to invalid value            #
########################################################################### 

# Due to "lost connection"
#SET @@global.key_buffer_size = -1;
#SELECT @@global.key_buffer_size;
#SET @@global.key_buffer_size = 100000000000;
#SELECT @@global.key_buffer_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.key_buffer_size = 10000.01;
SELECT @@global.key_buffer_size;
#SET @@global.key_buffer_size = -1024;
#SELECT @@global.key_buffer_size;
SET @@global.key_buffer_size = 4;
SELECT @@global.key_buffer_size = @min_key_buffer_size;
--echo 'Bug # 34837: Errors are not coming on assigning invalid values to variable';

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.key_buffer_size = ON;
SELECT @@global.key_buffer_size = @min_key_buffer_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.key_buffer_size = 'test';
SELECT @@global.key_buffer_size = @min_key_buffer_size;


--echo '#-------------------FN_DYNVARS_055_05----------------------------#'
########################################################################### 
#       Test if accessing session key_buffer_size gives error             #
########################################################################### 

--Error ER_GLOBAL_VARIABLE
SET @@session.key_buffer_size = 0;
SELECT @@key_buffer_size = @min_key_buffer_size;


--echo '#----------------------FN_DYNVARS_055_06------------------------#'
############################################################################## 
# Check if the value in GLOBAL & SESSION Tables matches values in variable   #
##############################################################################

SELECT @@global.key_buffer_size = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='key_buffer_size';

SELECT @@key_buffer_size = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='key_buffer_size';


--echo '#---------------------FN_DYNVARS_055_07----------------------#'
################################################################### 
#      Check if TRUE and FALSE values can be used on variable     #
################################################################### 

SET @@global.key_buffer_size = TRUE;
SELECT @@global.key_buffer_size = @min_key_buffer_size;
SET @@global.key_buffer_size = FALSE;
SELECT @@global.key_buffer_size = @min_key_buffer_size;


--echo '#---------------------FN_DYNVARS_055_08----------------------#'
#####################################################################
#  Check if accessing variable with SESSION,LOCAL and without SCOPE #
#  points to same session variable                                  #
#####################################################################

# due to differences in contents of the warnings
--disable_warnings
SET @@global.key_buffer_size = @min_key_buffer_size;
SELECT @@key_buffer_size = @@global.key_buffer_size;
--enable_warnings

--echo '#---------------------FN_DYNVARS_055_09----------------------#'
##########################################################################  
#   Check if key_buffer_size can be accessed with and without @@ sign    #
##########################################################################

--Error ER_GLOBAL_VARIABLE
SET key_buffer_size = @min_key_buffer_size;
SELECT @@key_buffer_size = @min_key_buffer_size;
--Error ER_PARSE_ERROR
SET local.key_buffer_size = 10;
--Error ER_UNKNOWN_TABLE
SELECT local.key_buffer_size;
--Error ER_PARSE_ERROR
SET global.key_buffer_size = 10;
--Error ER_UNKNOWN_TABLE
SELECT global.key_buffer_size;
--Error ER_BAD_FIELD_ERROR
SELECT key_buffer_size = @@session.key_buffer_size;


##############################  
#   Restore initial value    #
##############################

SET @@global.key_buffer_size = @start_value;
SELECT @@global.key_buffer_size;

--enable_warnings
#######################################################################
#              END OF key_buffer_size TESTS                           #
#######################################################################

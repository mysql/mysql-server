############## mysql-test\t\range_alloc_block_size_basic.test ###############
#                                                                             #
# Variable Name: range_alloc_block_size                                       #
# Scope: GLOBAL | SESSION                                                     #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value:2048 	                                                      #
# Max value: 4294967295                                                       #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable range_alloc_block_size   #
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

######################################################################
#           START OF range_alloc_block_size TESTS                    #
######################################################################


#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.range_alloc_block_size;
SELECT @start_global_value;
SET @start_session_value = @@session.range_alloc_block_size;
SELECT @start_session_value;


--echo '#--------------------FN_DYNVARS_137_01-------------------------#'
######################################################################
#     Display the DEFAULT value of range_alloc_block_size            #
######################################################################

SET @@global.range_alloc_block_size = 100;
SET @@global.range_alloc_block_size = DEFAULT;
SELECT @@global.range_alloc_block_size;

SET @@session.range_alloc_block_size = 200;
SET @@session.range_alloc_block_size = DEFAULT;
SELECT @@session.range_alloc_block_size;


--echo '#--------------------FN_DYNVARS_137_02-------------------------#'
######################################################################
#     Check the DEFAULT value of range_alloc_block_size              #
######################################################################

SET @@global.range_alloc_block_size = DEFAULT;
SELECT @@global.range_alloc_block_size = 2048;

SET @@session.range_alloc_block_size = DEFAULT;
SELECT @@session.range_alloc_block_size = 2048;


--echo '#--------------------FN_DYNVARS_137_03-------------------------#'
################################################################################
# Change the value of range_alloc_block_size to a valid value for GLOBAL Scope #
################################################################################

SET @@global.range_alloc_block_size = 2048;
SELECT @@global.range_alloc_block_size;
SET @@global.range_alloc_block_size = 4294967295;
SELECT @@global.range_alloc_block_size;
SET @@global.range_alloc_block_size = 4294967294;
SELECT @@global.range_alloc_block_size;


--echo '#--------------------FN_DYNVARS_137_04-------------------------#'
###################################################################################
# Change the value of range_alloc_block_size to a valid value for SESSION Scope #
###################################################################################
 
SET @@session.range_alloc_block_size = 2048;
SELECT @@session.range_alloc_block_size;
SET @@session.range_alloc_block_size = 4294967295;
SELECT @@session.range_alloc_block_size;
SET @@session.range_alloc_block_size = 4294967294;
SELECT @@session.range_alloc_block_size;


--echo '#------------------FN_DYNVARS_137_05-----------------------#'
##################################################################
# Change the value of range_alloc_block_size to an invalid value #
##################################################################

SET @@global.range_alloc_block_size = 0;
SELECT @@global.range_alloc_block_size;
SET @@global.range_alloc_block_size = -1024;
SELECT @@global.range_alloc_block_size;
SET @@global.range_alloc_block_size = 42949672951;
SELECT @@global.range_alloc_block_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.range_alloc_block_size = 65530.34;
SELECT @@global.range_alloc_block_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.range_alloc_block_size = test;
SELECT @@global.range_alloc_block_size;

SET @@session.range_alloc_block_size = 0;
SELECT @@session.range_alloc_block_size;
SET @@session.range_alloc_block_size = -2;
SELECT @@session.range_alloc_block_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.range_alloc_block_size = 65530.34;
SET @@session.range_alloc_block_size = 4294967296;
SELECT @@session.range_alloc_block_size;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.range_alloc_block_size = test;
SELECT @@session.range_alloc_block_size;


--echo '#------------------FN_DYNVARS_137_06-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################


SELECT @@global.range_alloc_block_size = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='range_alloc_block_size';

--echo '#------------------FN_DYNVARS_137_07-----------------------#'
####################################################################
#  Check if the value in SESSION Table matches value in variable   #
####################################################################

SELECT @@session.range_alloc_block_size = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='range_alloc_block_size';


--echo '#------------------FN_DYNVARS_137_08-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.range_alloc_block_size = TRUE;
SELECT @@global.range_alloc_block_size;
SET @@global.range_alloc_block_size = FALSE;
SELECT @@global.range_alloc_block_size;


--echo '#---------------------FN_DYNVARS_137_09----------------------#'
#################################################################################### 
#  Check if accessing variable with and without GLOBAL point to same variable   #
#################################################################################### 

SET @@global.range_alloc_block_size = 10;
SELECT @@range_alloc_block_size = @@global.range_alloc_block_size;


--echo '#---------------------FN_DYNVARS_137_10----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@range_alloc_block_size = 100;
SELECT @@range_alloc_block_size = @@local.range_alloc_block_size;
SELECT @@local.range_alloc_block_size = @@session.range_alloc_block_size;


--echo '#---------------------FN_DYNVARS_137_11----------------------#'
###################################################################################  
#   Check if range_alloc_block_size can be accessed with and without @@ sign    #
###################################################################################

SET range_alloc_block_size = 1;
SELECT @@range_alloc_block_size;
--Error ER_UNKNOWN_TABLE
SELECT local.range_alloc_block_size;
--Error ER_UNKNOWN_TABLE
SELECT session.range_alloc_block_size;
--Error ER_BAD_FIELD_ERROR
SELECT range_alloc_block_size = @@session.range_alloc_block_size;


####################################
#     Restore initial value        #
####################################

SET @@global.range_alloc_block_size = @start_global_value;
SELECT @@global.range_alloc_block_size;
SET @@session.range_alloc_block_size = @start_session_value;
SELECT @@session.range_alloc_block_size;


#############################################################
#                 END OF range_alloc_block_size TESTS     #
#############################################################


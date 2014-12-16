############## mysql-test\t\max_seeks_for_key_basic.test ###############
#                                                                             #
# Variable Name: max_seeks_for_key                                            #
# Scope: GLOBAL | SESSION                                                     #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value:  4294967295                                                  #
# Range:  1-4294967295                                                        #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable max_seeks_for_key        #
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

#################################################################
#           START OF max_seeks_for_key TESTS                    #
#################################################################


#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.max_seeks_for_key;
SELECT @start_global_value;
SET @start_session_value = @@session.max_seeks_for_key;
SELECT @start_session_value;


--echo '#--------------------FN_DYNVARS_083_01-------------------------#'
#################################################################
#     Display the DEFAULT value of max_seeks_for_key            #
#################################################################

SET @@global.max_seeks_for_key = 100;
SET @@global.max_seeks_for_key = DEFAULT;
SELECT @@global.max_seeks_for_key;

SET @@session.max_seeks_for_key = 200;
SET @@session.max_seeks_for_key = DEFAULT;
SELECT @@session.max_seeks_for_key;


--echo '#--------------------FN_DYNVARS_083_02-------------------------#'
#################################################################
#     Check the DEFAULT value of max_seeks_for_key              #
#################################################################

SET @@global.max_seeks_for_key = DEFAULT;
SELECT @@global.max_seeks_for_key = 4294967295;

SET @@session.max_seeks_for_key = DEFAULT;
SELECT @@session.max_seeks_for_key = 4294967295;


--echo '#--------------------FN_DYNVARS_083_03-------------------------#'
###########################################################################
# Change the value of max_seeks_for_key to a valid value for GLOBAL Scope #
###########################################################################

SET @@global.max_seeks_for_key = 1;
SELECT @@global.max_seeks_for_key;
SET @@global.max_seeks_for_key = 2;
SELECT @@global.max_seeks_for_key;
SET @@global.max_seeks_for_key = 65536;
SELECT @@global.max_seeks_for_key;
SET @@global.max_seeks_for_key = 4294967295;
SELECT @@global.max_seeks_for_key;
SET @@global.max_seeks_for_key = 4294967294;
SELECT @@global.max_seeks_for_key;


--echo '#--------------------FN_DYNVARS_083_04-------------------------#'
############################################################################
# Change the value of max_seeks_for_key to a valid value for SESSION Scope #
############################################################################
SET @@session.max_seeks_for_key = 1;
SELECT @@session.max_seeks_for_key;
SET @@session.max_seeks_for_key = 2;
SELECT @@session.max_seeks_for_key;
SET @@session.max_seeks_for_key = 4294967295;
SELECT @@session.max_seeks_for_key;
SET @@session.max_seeks_for_key = 4294967294;
SELECT @@session.max_seeks_for_key;
SET @@session.max_seeks_for_key = 65535;
SELECT @@session.max_seeks_for_key;


--echo '#------------------FN_DYNVARS_083_05-----------------------#'
#############################################################
# Change the value of max_seeks_for_key to an invalid value #
#############################################################

SET @@global.max_seeks_for_key = 0;
SELECT @@global.max_seeks_for_key;
SET @@global.max_seeks_for_key = -1024;
SELECT @@global.max_seeks_for_key;
SET @@global.max_seeks_for_key = 4294967296;
SELECT @@global.max_seeks_for_key;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.max_seeks_for_key = 65530.34;
SELECT @@global.max_seeks_for_key;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.max_seeks_for_key = test;
SELECT @@global.max_seeks_for_key;

SET @@session.max_seeks_for_key = 0;
SELECT @@session.max_seeks_for_key;
SET @@session.max_seeks_for_key = -2;
SELECT @@session.max_seeks_for_key;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.max_seeks_for_key = 65530.34;
SET @@session.max_seeks_for_key = 4294967296;
SELECT @@session.max_seeks_for_key;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.max_seeks_for_key = test;
SELECT @@session.max_seeks_for_key;


--echo '#------------------FN_DYNVARS_083_06-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################

--disable_warnings
SELECT @@global.max_seeks_for_key = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='max_seeks_for_key';
--enable_warnings

--echo '#------------------FN_DYNVARS_083_07-----------------------#'
####################################################################
#  Check if the value in SESSION Table matches value in variable   #
####################################################################

--disable_warnings
SELECT @@session.max_seeks_for_key = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='max_seeks_for_key';
--enable_warnings

--echo '#------------------FN_DYNVARS_083_08-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.max_seeks_for_key = TRUE;
SELECT @@global.max_seeks_for_key;
SET @@global.max_seeks_for_key = FALSE;
SELECT @@global.max_seeks_for_key;


--echo '#---------------------FN_DYNVARS_083_09----------------------#'
#################################################################################
#  Check if accessing variable with and without GLOBAL point to same variable   #
#################################################################################

SET @@global.max_seeks_for_key = 10;
SELECT @@max_seeks_for_key = @@global.max_seeks_for_key;


--echo '#---------------------FN_DYNVARS_083_10----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@max_seeks_for_key = 100;
SELECT @@max_seeks_for_key = @@local.max_seeks_for_key;
SELECT @@local.max_seeks_for_key = @@session.max_seeks_for_key;


--echo '#---------------------FN_DYNVARS_083_11----------------------#'
############################################################################  
#   Check if max_seeks_for_key can be accessed with and without @@ sign    #
############################################################################

SET max_seeks_for_key = 1;
SELECT @@max_seeks_for_key;
--Error ER_UNKNOWN_TABLE
SELECT local.max_seeks_for_key;
--Error ER_UNKNOWN_TABLE
SELECT session.max_seeks_for_key;
--Error ER_BAD_FIELD_ERROR
SELECT max_seeks_for_key = @@session.max_seeks_for_key;


####################################
#     Restore initial value        #
####################################

SET @@global.max_seeks_for_key = @start_global_value;
SELECT @@global.max_seeks_for_key;
SET @@session.max_seeks_for_key = @start_session_value;
SELECT @@session.max_seeks_for_key;


#############################################################
#                 END OF max_seeks_for_key TESTS     #
#############################################################


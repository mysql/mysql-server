############## mysql-test\t\bulk_insert_buffer_size_basic.test #################
#                                                                              #
# Variable Name: bulk_insert_buffer_size                                       #
# Scope: GLOBAL & SESSION                                                      #
# Access Type: Dynamic                                                         #
# Data Type: Numeric                                                           #
# Default Value: 8388608                                                       #
# Range: 0 - 4294967295                                                        #
#                                                                              #
#                                                                              #
# Creation Date: 2008-02-07                                                    #
# Author:  Salman Rawala                                                       #
#                                                                              #
# Description: Test Cases of Dynamic System Variable "bulk_insert_buffer_size" #
#              that checks behavior of this variable in the following ways     #
#              * Default Value                                                 #
#              * Valid & Invalid values                                        #
#              * Scope & Access method                                         #
#              * Data Integrity                          .                     #
#                                                                              #
# Reference: http://dev.mysql.com/doc/refman/5.1/en/                           # 
#   server-system-variables.html#option_mysqld_bulk_insert_buffer_size         #
#                                                                              #
################################################################################

--source include/load_sysvars.inc

#######################################################################
#           START OF bulk_insert_buffer_size TESTS                    #
#######################################################################

#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.bulk_insert_buffer_size;
SELECT @start_global_value;
SET @start_session_value = @@session.bulk_insert_buffer_size;
SELECT @start_session_value;

--echo '#--------------------FN_DYNVARS_007_01-------------------------#'
#######################################################################
#     Display the DEFAULT value of bulk_insert_buffer_size            #
#######################################################################

SET @@global.bulk_insert_buffer_size = 100;
SET @@global.bulk_insert_buffer_size = DEFAULT;
SELECT @@global.bulk_insert_buffer_size;

SET @@session.bulk_insert_buffer_size = 200;
SET @@session.bulk_insert_buffer_size = DEFAULT;
SELECT @@session.bulk_insert_buffer_size;


--echo '#--------------------FN_DYNVARS_007_02-------------------------#'
#######################################################################
#     Check the DEFAULT value of bulk_insert_buffer_size              #
#######################################################################

SET @@global.bulk_insert_buffer_size = @start_global_value;
SELECT @@global.bulk_insert_buffer_size = 8388608;

SET @@session.bulk_insert_buffer_size = @start_session_value;
SELECT @@session.bulk_insert_buffer_size = 8388608;


--echo '#--------------------FN_DYNVARS_007_03-------------------------#'
###############################################################################
#Change the value of bulk_insert_buffer_size to valid values for GLOBAL Scope #
###############################################################################

SET @@global.bulk_insert_buffer_size = 0;
SELECT @@global.bulk_insert_buffer_size;
SET @@global.bulk_insert_buffer_size = 1;
SELECT @@global.bulk_insert_buffer_size;
SET @@global.bulk_insert_buffer_size = 4294967295;
SELECT @@global.bulk_insert_buffer_size;
SET @@global.bulk_insert_buffer_size = 429496;
SELECT @@global.bulk_insert_buffer_size;


--echo '#--------------------FN_DYNVARS_007_04-------------------------#'
###############################################################################
#Change the value of bulk_insert_buffer_size to valid values for SESSION Scope#
###############################################################################

SET @@session.bulk_insert_buffer_size = 0;
SELECT @@session.bulk_insert_buffer_size;
SET @@session.bulk_insert_buffer_size = 1;
SELECT @@session.bulk_insert_buffer_size;
SET @@session.bulk_insert_buffer_size = 4294967295;
SELECT @@session.bulk_insert_buffer_size;
SET @@session.bulk_insert_buffer_size = 429496;
SELECT @@session.bulk_insert_buffer_size;


--echo '#------------------FN_DYNVARS_007_05-----------------------#'
###################################################################
# Change the value of bulk_insert_buffer_size to an invalid value #
###################################################################

SET @@global.bulk_insert_buffer_size = 42949672950;
SELECT @@global.bulk_insert_buffer_size;
SET @@global.bulk_insert_buffer_size = -1024;
SELECT @@global.bulk_insert_buffer_size;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.bulk_insert_buffer_size = test;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.bulk_insert_buffer_size = ON;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.bulk_insert_buffer_size = 429496.10;

SET @@session.bulk_insert_buffer_size = 42949672950;
SELECT @@session.bulk_insert_buffer_size;
SET @@session.bulk_insert_buffer_size = -2;
SELECT @@session.bulk_insert_buffer_size;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.bulk_insert_buffer_size = test;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.bulk_insert_buffer_size = 429496.10;


--echo '#------------------FN_DYNVARS_007_06-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################

SELECT @@global.bulk_insert_buffer_size = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='bulk_insert_buffer_size';

--echo '#------------------FN_DYNVARS_007_07-----------------------#'
####################################################################
#  Check if the value in SESSION Table matches value in variable   #
####################################################################

SELECT @@session.bulk_insert_buffer_size = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='bulk_insert_buffer_size';


--echo '#------------------FN_DYNVARS_007_08-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.bulk_insert_buffer_size = TRUE;
SELECT @@global.bulk_insert_buffer_size;
SET @@global.bulk_insert_buffer_size = FALSE;
SELECT @@global.bulk_insert_buffer_size;

SET @@session.bulk_insert_buffer_size = TRUE;
SELECT @@session.bulk_insert_buffer_size;
SET @@session.bulk_insert_buffer_size = FALSE;
SELECT @@session.bulk_insert_buffer_size;


--echo '#---------------------FN_DYNVARS_007_09----------------------#'
##############################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points #
#    to same session variable                                                #
##############################################################################

SET @@bulk_insert_buffer_size = 100;
SELECT @@bulk_insert_buffer_size = @@local.bulk_insert_buffer_size;
SELECT @@local.bulk_insert_buffer_size = @@session.bulk_insert_buffer_size;


--echo '#---------------------FN_DYNVARS_007_10----------------------#'
###############################################################################
#   Check if bulk_insert_buffer_size can be accessed with and without @@ sign #
###############################################################################

SET bulk_insert_buffer_size = 1;
SELECT @@bulk_insert_buffer_size;
--Error ER_PARSE_ERROR
SET local.bulk_insert_buffer_size = 1;
--Error ER_UNKNOWN_TABLE
SELECT local.bulk_insert_buffer_size;
--Error ER_PARSE_ERROR
SET session.bulk_insert_buffer_size = 1;
--Error ER_UNKNOWN_TABLE
SELECT session.bulk_insert_buffer_size;
--Error ER_BAD_FIELD_ERROR
SELECT bulk_insert_buffer_size = @@session.bulk_insert_buffer_size;


####################################
#     Restore initial value        #
####################################

SET @@global.bulk_insert_buffer_size = @start_global_value;
SELECT @@global.bulk_insert_buffer_size;
SET @@session.bulk_insert_buffer_size = @start_session_value;
SELECT @@session.bulk_insert_buffer_size;


####################################################
#         END OF bulk_insert_buffer_size TESTS     #
####################################################

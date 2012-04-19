############## mysql-test\t\max_write_lock_count_basic.test ###############
#                                                                             #
# Variable Name: max_write_lock_count                                         #
# Scope: GLOBAL                                                               #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value: 4294967295                                                   #
# Range:1-4294967295                                                          #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable max_write_lock_count     #
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

##########################################
#   START OF max_write_lock_count TESTS  #
##########################################


#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.max_write_lock_count;
SELECT @start_global_value;


--echo '#--------------------FN_DYNVARS_088_01-------------------------#'
############################################################
#    Display the DEFAULT value of max_write_lock_count     #
############################################################

SET @@global.max_write_lock_count = 1000;
SET @@global.max_write_lock_count = DEFAULT;
SELECT @@global.max_write_lock_count;


--echo '#--------------------FN_DYNVARS_088_02-------------------------#'
############################################################
#     Check the DEFAULT value of max_write_lock_count      #
############################################################

SET @@global.max_write_lock_count = DEFAULT;
SELECT @@global.max_write_lock_count = 4294967295;


--echo '#--------------------FN_DYNVARS_088_03-------------------------#'
##############################################################################
# Change the value of max_write_lock_count to a valid value for GLOBAL Scope #
##############################################################################

SET @@global.max_write_lock_count = 1;
SELECT @@global.max_write_lock_count;
SET @@global.max_write_lock_count = 2;
SELECT @@global.max_write_lock_count;
SET @@global.max_write_lock_count = 65536;
SELECT @@global.max_write_lock_count;
SET @@global.max_write_lock_count = 4294967295;
SELECT @@global.max_write_lock_count;
SET @@global.max_write_lock_count = 4294967294;
SELECT @@global.max_write_lock_count;



--echo '#------------------FN_DYNVARS_088_04-----------------------#'
################################################################
# Change the value of max_write_lock_count to an invalid value #
################################################################

SET @@global.max_write_lock_count = -1024;
SELECT @@global.max_write_lock_count;
SET @@global.max_write_lock_count = 4294967296;
SELECT @@global.max_write_lock_count;
SET @@global.max_write_lock_count = -1;
SELECT @@global.max_write_lock_count;
SET @@global.max_write_lock_count = 429496729500;
SELECT @@global.max_write_lock_count;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.max_write_lock_count = 65530.34;
SELECT @@global.max_write_lock_count;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.max_write_lock_count = test;
SELECT @@global.max_write_lock_count;



--echo '#------------------FN_DYNVARS_088_05-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################


SELECT @@global.max_write_lock_count = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='max_write_lock_count';


--echo '#------------------FN_DYNVARS_088_06-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.max_write_lock_count = TRUE;
SELECT @@global.max_write_lock_count;
SET @@global.max_write_lock_count = FALSE;
SELECT @@global.max_write_lock_count;


--echo '#---------------------FN_DYNVARS_088_07----------------------#'
#################################################################################
#  Check if accessing variable with and without GLOBAL point to same variable   #
#################################################################################

SET @@global.max_write_lock_count = 20;
SELECT @@max_write_lock_count = @@global.max_write_lock_count;


--echo '#---------------------FN_DYNVARS_088_08----------------------#'
###############################################################################
#   Check if max_write_lock_count can be accessed with and without @@ sign    #
###############################################################################


SET @@global.max_write_lock_count = 102;
SELECT @@max_write_lock_count;
--Error ER_UNKNOWN_TABLE
SELECT local.max_write_lock_count;
--Error ER_UNKNOWN_TABLE
SELECT global.max_write_lock_count;
--Error ER_BAD_FIELD_ERROR
SELECT max_write_lock_count = @@global.max_write_lock_count;


####################################
#     Restore initial value        #
####################################

SET @@global.max_write_lock_count = @start_global_value;
SELECT @@global.max_write_lock_count;


###################################################
#           END OF max_write_lock_count TESTS     #
###################################################


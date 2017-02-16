############## mysql-test\t\net_retry_count_basic.test ###############
#                                                                             #
# Variable Name: net_retry_count                                              #
# Scope: GLOBAL | SESSION                                                     #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value:10                                                            #
# Range: 1-4294967295                                                         #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable net_retry_count          #
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

###############################################################
#           START OF net_retry_count TESTS                    #
###############################################################


#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.net_retry_count;
SELECT @start_global_value;
SET @start_session_value = @@session.net_retry_count;
SELECT @start_session_value;


--echo '#--------------------FN_DYNVARS_111_01-------------------------#'
###############################################################
#     Display the DEFAULT value of net_retry_count            #
###############################################################

SET @@global.net_retry_count = 100;
SET @@global.net_retry_count = DEFAULT;
SELECT @@global.net_retry_count;

SET @@session.net_retry_count = 200;
SET @@session.net_retry_count = DEFAULT;
SELECT @@session.net_retry_count;


--echo '#--------------------FN_DYNVARS_111_02-------------------------#'
###############################################################
#     Check the DEFAULT value of net_retry_count              #
###############################################################

SET @@global.net_retry_count = DEFAULT;
SELECT @@global.net_retry_count = 10;

SET @@session.net_retry_count = DEFAULT;
SELECT @@session.net_retry_count = 10;


--echo '#--------------------FN_DYNVARS_111_03-------------------------#'
#########################################################################
# Change the value of net_retry_count to a valid value for GLOBAL Scope #
#########################################################################

SET @@global.net_retry_count = 1;
SELECT @@global.net_retry_count;
SET @@global.net_retry_count = 2;
SELECT @@global.net_retry_count;
SET @@global.net_retry_count = 4294967295;
SELECT @@global.net_retry_count;
SET @@global.net_retry_count = 4294967294;
SELECT @@global.net_retry_count;
SET @@global.net_retry_count = 65536;
SELECT @@global.net_retry_count;

--echo '#--------------------FN_DYNVARS_111_04-------------------------#'
##########################################################################
# Change the value of net_retry_count to a valid value for SESSION Scope #
##########################################################################
 
SET @@session.net_retry_count = 1;
SELECT @@session.net_retry_count;
SET @@session.net_retry_count = 2;
SELECT @@session.net_retry_count;
SET @@session.net_retry_count = 65535;
SELECT @@session.net_retry_count;
SET @@session.net_retry_count = 4294967295;
SELECT @@session.net_retry_count;
SET @@session.net_retry_count = 4294967294;
SELECT @@session.net_retry_count;

--echo '#------------------FN_DYNVARS_111_05-----------------------#'
###########################################################
# Change the value of net_retry_count to an invalid value #
###########################################################

SET @@global.net_retry_count = 0;
SELECT @@global.net_retry_count;
SET @@global.net_retry_count = -1024;
SELECT @@global.net_retry_count;
SET @@global.net_retry_count = 4294967296;
SELECT @@global.net_retry_count;
SET @@global.net_retry_count = 429496729500;
SELECT @@global.net_retry_count;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.net_retry_count = 65530.34;
SELECT @@global.net_retry_count;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.net_retry_count = test;
SELECT @@global.net_retry_count;

SET @@session.net_retry_count = 0;
SELECT @@session.net_retry_count;
SET @@session.net_retry_count = -2;
SELECT @@session.net_retry_count;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.net_retry_count = 65530.34;
SET @@session.net_retry_count = 6555015425;
SELECT @@session.net_retry_count;
SET @@session.net_retry_count = 4294967296;
SELECT @@session.net_retry_count;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.net_retry_count = test;
SELECT @@session.net_retry_count;


--echo '#------------------FN_DYNVARS_111_06-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################


SELECT @@global.net_retry_count = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='net_retry_count';

--echo '#------------------FN_DYNVARS_111_07-----------------------#'
####################################################################
#  Check if the value in SESSION Table matches value in variable   #
####################################################################

SELECT @@session.net_retry_count = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='net_retry_count';


--echo '#------------------FN_DYNVARS_111_08-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.net_retry_count = TRUE;
SELECT @@global.net_retry_count;
SET @@global.net_retry_count = FALSE;
SELECT @@global.net_retry_count;


--echo '#---------------------FN_DYNVARS_111_09----------------------#'
################################################################################# 
#  Check if accessing variable with and without GLOBAL point to same variable   #
################################################################################# 

SET @@global.net_retry_count = 10;
SELECT @@net_retry_count = @@global.net_retry_count;


--echo '#---------------------FN_DYNVARS_111_10----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@net_retry_count = 100;
SELECT @@net_retry_count = @@local.net_retry_count;
SELECT @@local.net_retry_count = @@session.net_retry_count;


--echo '#---------------------FN_DYNVARS_111_11----------------------#'
##########################################################################
#   Check if net_retry_count can be accessed with and without @@ sign    #
##########################################################################

SET net_retry_count = 1;
SELECT @@net_retry_count;
--Error ER_UNKNOWN_TABLE
SELECT local.net_retry_count;
--Error ER_UNKNOWN_TABLE
SELECT session.net_retry_count;
--Error ER_BAD_FIELD_ERROR
SELECT net_retry_count = @@session.net_retry_count;


####################################
#     Restore initial value        #
####################################

SET @@global.net_retry_count = @start_global_value;
SELECT @@global.net_retry_count;
SET @@session.net_retry_count = @start_session_value;
SELECT @@session.net_retry_count;


####################################################
#                 END OF net_retry_count TESTS     #
####################################################


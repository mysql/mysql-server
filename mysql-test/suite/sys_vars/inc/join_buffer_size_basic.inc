############## mysql-test\t\join_buffer_size_basic.test ###############
#                                                                             #
# Variable Name: join_buffer_size                                             #
# Scope: GLOBAL | SESSION                                                     #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value: 262144                                                       #
# Range: 8200-4294967295                                                      #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable join_buffer_size         #
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

################################################################
#           START OF join_buffer_size TESTS                    #
################################################################


#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.join_buffer_size;
SELECT @start_global_value;
SET @start_session_value = @@session.join_buffer_size;
SELECT @start_session_value;


--echo '#--------------------FN_DYNVARS_053_01-------------------------#'
################################################################
#     Display the DEFAULT value of join_buffer_size            #
################################################################

SET @@global.join_buffer_size = DEFAULT;
SELECT @@global.join_buffer_size;

SET @@session.join_buffer_size = DEFAULT;
SELECT @@session.join_buffer_size;


--echo '#--------------------FN_DYNVARS_053_03-------------------------#'
##########################################################################
# Change the value of join_buffer_size to a valid value for GLOBAL Scope #
##########################################################################

SET @@global.join_buffer_size = 8200;
SELECT @@global.join_buffer_size;
SET @@global.join_buffer_size = 65536;
SELECT @@global.join_buffer_size;
SET @@global.join_buffer_size = 4294967295;
SELECT @@global.join_buffer_size;

--echo '#--------------------FN_DYNVARS_053_04-------------------------#'
###########################################################################
# Change the value of join_buffer_size to a valid value for SESSION Scope #
###########################################################################
 
SET @@session.join_buffer_size = 8200;
SELECT @@session.join_buffer_size;
SET @@session.join_buffer_size = 65536;
SELECT @@session.join_buffer_size;
SET @@session.join_buffer_size = 4294967295;
SELECT @@session.join_buffer_size;

--echo '#------------------FN_DYNVARS_053_05-----------------------#'
############################################################
# Change the value of join_buffer_size to an invalid value #
############################################################

SET @@global.join_buffer_size = 0;
SELECT @@global.join_buffer_size;
SET @@global.join_buffer_size = -1024;
SELECT @@global.join_buffer_size;
SET @@global.join_buffer_size = 127;
SELECT @@global.join_buffer_size;
SET @@global.join_buffer_size = 42949672951;
SELECT @@global.join_buffer_size;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.join_buffer_size = 65530.34;
SELECT @@global.join_buffer_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.join_buffer_size = test;
SELECT @@global.join_buffer_size;

SET @@session.join_buffer_size = 0;
SELECT @@session.join_buffer_size;
SET @@session.join_buffer_size = -1024;
SELECT @@session.join_buffer_size;
SET @@session.join_buffer_size = 127;
SELECT @@session.join_buffer_size;
SET @@session.join_buffer_size = 42949672951;
SELECT @@session.join_buffer_size;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.join_buffer_size = 65530.34;
SELECT @@session.join_buffer_size;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.join_buffer_size = test;
SELECT @@session.join_buffer_size;

--echo '#------------------FN_DYNVARS_053_06-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################


SELECT @@global.join_buffer_size = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='join_buffer_size';

--echo '#------------------FN_DYNVARS_053_07-----------------------#'
####################################################################
#  Check if the value in SESSION Table matches value in variable   #
####################################################################

SELECT @@session.join_buffer_size = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES
WHERE VARIABLE_NAME='join_buffer_size';


--echo '#------------------FN_DYNVARS_053_08-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.join_buffer_size = TRUE;
SET @@global.join_buffer_size = FALSE;

--echo '#---------------------FN_DYNVARS_001_09----------------------#'
#################################################################################
#  Check if accessing variable with and without GLOBAL point to same variable   #
#################################################################################

SET @@global.join_buffer_size = 10;
SELECT @@join_buffer_size = @@global.join_buffer_size;

--echo '#---------------------FN_DYNVARS_001_10----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@join_buffer_size = 100;
SELECT @@join_buffer_size = @@local.join_buffer_size;
SELECT @@local.join_buffer_size = @@session.join_buffer_size;

--echo '#---------------------FN_DYNVARS_001_11----------------------#'
##############################################################################
#   Check if join_buffer_size can be accessed with and without @@ sign    #
##############################################################################

SET join_buffer_size = 1;
SELECT @@join_buffer_size;
--Error ER_UNKNOWN_TABLE
SELECT local.join_buffer_size;
--Error ER_UNKNOWN_TABLE
SELECT session.join_buffer_size;
--Error ER_BAD_FIELD_ERROR
SELECT join_buffer_size = @@session.join_buffer_size;

####################################
#     Restore initial value        #
####################################

SET @@global.join_buffer_size = @start_global_value;
SELECT @@global.join_buffer_size;
SET @@session.join_buffer_size = @start_session_value;
SELECT @@session.join_buffer_size;


########################################################
#                 END OF join_buffer_size TESTS     #
########################################################


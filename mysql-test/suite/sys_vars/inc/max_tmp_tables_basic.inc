############## mysql-test\t\max_tmp_tables_basic.test ###############
#                                                                             #
# Variable Name: max_tmp_tables                                               #
# Scope: GLOBAL | SESSION                                                     #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value: 32                                                           #
# Range: 1-4294967295                                                         #
#                                                                             # 
#                                                                             #
#                                                                             #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable max_tmp_tables           #
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

####################################
#   START OF max_tmp_tables TESTS  #
####################################


#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.max_tmp_tables;
SELECT @start_global_value;
SET @start_session_value = @@session.max_tmp_tables;
SELECT @start_session_value;


--echo '#--------------------FN_DYNVARS_086_01-------------------------#'
######################################################
#    Display the DEFAULT value of max_tmp_tables     #
######################################################

SET @@global.max_tmp_tables = 1000;
SET @@global.max_tmp_tables = DEFAULT;
SELECT @@global.max_tmp_tables;


SET @@session.max_tmp_tables = 1000;
SET @@session.max_tmp_tables = DEFAULT;
SELECT @@session.max_tmp_tables;


--echo '#--------------------FN_DYNVARS_086_02-------------------------#'
######################################################
#     Check the DEFAULT value of max_tmp_tables      #
######################################################

SET @@global.max_tmp_tables = DEFAULT;
SELECT @@global.max_tmp_tables = 32;

SET @@session.max_tmp_tables = DEFAULT;
SELECT @@session.max_tmp_tables = 32;



--echo '#--------------------FN_DYNVARS_086_03-------------------------#'
########################################################################
# Change the value of max_tmp_tables to a valid value for GLOBAL Scope #
########################################################################

SET @@global.max_tmp_tables = 1;
SELECT @@global.max_tmp_tables;
SET @@global.max_tmp_tables = 2;
SELECT @@global.max_tmp_tables;
SET @@global.max_tmp_tables = 65536;
SELECT @@global.max_tmp_tables;
SET @@global.max_tmp_tables = 4294967295;
SELECT @@global.max_tmp_tables;
SET @@global.max_tmp_tables = 4294967294;
SELECT @@global.max_tmp_tables;

--echo '#--------------------FN_DYNVARS_086_04-------------------------#'
#########################################################################
# Change the value of max_tmp_tables to a valid value for SESSION Scope #
#########################################################################

SET @@session.max_tmp_tables = 1;
SELECT @@session.max_tmp_tables;

SET @@session.max_tmp_tables = 2;
SELECT @@session.max_tmp_tables;

SET @@session.max_tmp_tables = 65536;
SELECT @@session.max_tmp_tables;

SET @@session.max_tmp_tables = 4294967295;
SELECT @@session.max_tmp_tables;

SET @@session.max_tmp_tables = 4294967294;
SELECT @@session.max_tmp_tables;


--echo '#------------------FN_DYNVARS_086_05-----------------------#'
##########################################################
# Change the value of max_tmp_tables to an invalid value #
##########################################################

SET @@global.max_tmp_tables = -1024;
SELECT @@global.max_tmp_tables;
SET @@global.max_tmp_tables = 4294967296;
SELECT @@global.max_tmp_tables;
SET @@global.max_tmp_tables = -1;
SELECT @@global.max_tmp_tables;
SET @@global.max_tmp_tables = 429496729500;
SELECT @@global.max_tmp_tables;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.max_tmp_tables = 65530.34;
SELECT @@global.max_tmp_tables;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.max_tmp_tables = test;
SELECT @@global.max_tmp_tables;

SET @@session.max_tmp_tables = 4294967296;
SELECT @@session.max_tmp_tables;
SET @@session.max_tmp_tables = -1;
SELECT @@session.max_tmp_tables;
SET @@session.max_tmp_tables = 429496729500;
SELECT @@session.max_tmp_tables;
SET @@session.max_tmp_tables = -001;
SELECT @@session.max_tmp_tables;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.max_tmp_tables = 65530.34;
SET @@session.max_tmp_tables = 10737418241;
SELECT @@session.max_tmp_tables;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.max_tmp_tables = test;
SELECT @@session.max_tmp_tables;


--echo '#------------------FN_DYNVARS_086_06-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################

--disable_warnings
SELECT VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='max_tmp_tables'
into @tmp;
--enable_warnings

SELECT @@global.max_tmp_tables = @tmp;

--echo '#------------------FN_DYNVARS_086_07-----------------------#'
####################################################################
#  Check if the value in SESSION Table matches value in variable   #
####################################################################

--disable_warnings
SELECT VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='max_tmp_tables'
into @tmp;
--enable_warnings

SELECT @@session.max_tmp_tables = @tmp;

--echo '#------------------FN_DYNVARS_086_08-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.max_tmp_tables = TRUE;
SELECT @@global.max_tmp_tables;
SET @@global.max_tmp_tables = FALSE;
SELECT @@global.max_tmp_tables;


--echo '#---------------------FN_DYNVARS_086_09----------------------#'
#################################################################################
#  Check if accessing variable with and without GLOBAL point to same variable   #
#################################################################################

SET @@global.max_tmp_tables = 20;
SELECT @@max_tmp_tables = @@global.max_tmp_tables;


--echo '#---------------------FN_DYNVARS_086_10----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@max_tmp_tables = 255;
SELECT @@max_tmp_tables = @@local.max_tmp_tables;
SELECT @@local.max_tmp_tables = @@session.max_tmp_tables;


--echo '#---------------------FN_DYNVARS_086_11----------------------#'
#########################################################################
#   Check if max_tmp_tables can be accessed with and without @@ sign    #
#########################################################################


SET max_tmp_tables = 102;
SELECT @@max_tmp_tables;
--Error ER_UNKNOWN_TABLE
SELECT local.max_tmp_tables;
--Error ER_UNKNOWN_TABLE
SELECT session.max_tmp_tables;
--Error ER_BAD_FIELD_ERROR
SELECT max_tmp_tables = @@session.max_tmp_tables;


####################################
#     Restore initial value        #
####################################

SET @@global.max_tmp_tables = @start_global_value;
SELECT @@global.max_tmp_tables;
SET @@session.max_tmp_tables = @start_session_value;
SELECT @@session.max_tmp_tables;


###################################################
#                 END OF max_tmp_tables TESTS     #
###################################################


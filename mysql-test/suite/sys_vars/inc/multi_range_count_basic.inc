############## mysql-test\t\multi_range_count_basic.test ###############
#                                                                             #
# Variable Name: multi_range_count                                            #
# Scope: GLOBAL | SESSION                                                     #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value:  256                                                         #
# Range:1-4294967295                                                          #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable multi_range_count        #
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
#           START OF multi_range_count TESTS                    #
#################################################################


#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.multi_range_count;
SELECT @start_global_value;
SET @start_session_value = @@session.multi_range_count;
SELECT @start_session_value;


--echo '#--------------------FN_DYNVARS_090_01-------------------------#'
#################################################################
#     Display the DEFAULT value of multi_range_count            #
#################################################################

SET @@global.multi_range_count = 100;
SET @@global.multi_range_count = DEFAULT;
SELECT @@global.multi_range_count;

SET @@session.multi_range_count = 200;
SET @@session.multi_range_count = DEFAULT;
SELECT @@session.multi_range_count;


--echo '#--------------------FN_DYNVARS_090_02-------------------------#'
#################################################################
#     Check the DEFAULT value of multi_range_count              #
#################################################################

SET @@global.multi_range_count = DEFAULT;
SELECT @@global.multi_range_count = 256;

SET @@session.multi_range_count = DEFAULT;
SELECT @@session.multi_range_count = 256;


--echo '#--------------------FN_DYNVARS_090_03-------------------------#'
###########################################################################
# Change the value of multi_range_count to a valid value for GLOBAL Scope #
###########################################################################

SET @@global.multi_range_count = 1;
SELECT @@global.multi_range_count;
SET @@global.multi_range_count = 60020;
SELECT @@global.multi_range_count;
SET @@global.multi_range_count = 65535;
SELECT @@global.multi_range_count;
SET @@global.multi_range_count = 4294967295;
SELECT @@global.multi_range_count;
SET @@global.multi_range_count = 4294967294;
SELECT @@global.multi_range_count;



--echo '#--------------------FN_DYNVARS_090_04-------------------------#'
############################################################################
# Change the value of multi_range_count to a valid value for SESSION Scope #
############################################################################
 
SET @@session.multi_range_count = 1;
SELECT @@session.multi_range_count;
SET @@session.multi_range_count = 50050;
SELECT @@session.multi_range_count;
SET @@session.multi_range_count = 65535;
SELECT @@session.multi_range_count;
SET @@session.multi_range_count = 4294967295;
SELECT @@session.multi_range_count;
SET @@session.multi_range_count = 4294967294;
SELECT @@session.multi_range_count;


--echo '#------------------FN_DYNVARS_090_05-----------------------#'
#############################################################
# Change the value of multi_range_count to an invalid value #
#############################################################

SET @@global.multi_range_count = 0;
SELECT @@global.multi_range_count;
SET @@global.multi_range_count = 4294967296;
SELECT @@global.multi_range_count;
SET @@global.multi_range_count = -1024;
SELECT @@global.multi_range_count;
SET @@global.multi_range_count = 429496729500;
SELECT @@global.multi_range_count;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.multi_range_count = 65530.34;
SELECT @@global.multi_range_count;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.multi_range_count = test;
SELECT @@global.multi_range_count;

SET @@session.multi_range_count = 0;
SELECT @@session.multi_range_count;
SET @@session.multi_range_count = 4294967296;
SELECT @@session.multi_range_count;
SET @@session.multi_range_count = -1;
SELECT @@session.multi_range_count;
--Error ER_PARSE_ERROR
SET @@session.multi_range_count = 65530.34.;
SET @@session.multi_range_count = 4294967295021;
SELECT @@session.multi_range_count;
--echo 'Bug # 34837: Errors are not coming on assigning invalid values to variable';

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.multi_range_count = test;
SELECT @@session.multi_range_count;


--echo '#------------------FN_DYNVARS_090_06-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################


SELECT @@global.multi_range_count = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='multi_range_count';

--echo '#------------------FN_DYNVARS_090_07-----------------------#'
####################################################################
#  Check if the value in SESSION Table matches value in variable   #
####################################################################

SELECT @@session.multi_range_count = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='multi_range_count';


--echo '#------------------FN_DYNVARS_090_08-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.multi_range_count = TRUE;
SELECT @@global.multi_range_count;
SET @@global.multi_range_count = FALSE;
SELECT @@global.multi_range_count;


--echo '#---------------------FN_DYNVARS_090_09----------------------#'
################################################################################# 
#  Check if accessing variable with and without GLOBAL point to same variable   #
################################################################################# 

SET @@global.multi_range_count = 10;
SELECT @@multi_range_count = @@global.multi_range_count;


--echo '#---------------------FN_DYNVARS_090_10----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@multi_range_count = 100;
SELECT @@multi_range_count = @@local.multi_range_count;
SELECT @@local.multi_range_count = @@session.multi_range_count;


--echo '#---------------------FN_DYNVARS_090_11----------------------#'
############################################################################  
#   Check if multi_range_count can be accessed with and without @@ sign    #
############################################################################

SET multi_range_count = 1;
SELECT @@multi_range_count;
--Error ER_UNKNOWN_TABLE
SELECT local.multi_range_count;
--Error ER_UNKNOWN_TABLE
SELECT session.multi_range_count;
--Error ER_BAD_FIELD_ERROR
SELECT multi_range_count = @@session.multi_range_count;


####################################
#     Restore initial value        #
####################################

SET @@global.multi_range_count = @start_global_value;
SELECT @@global.multi_range_count;
SET @@session.multi_range_count = @start_session_value;
SELECT @@session.multi_range_count;


######################################################
#                 END OF multi_range_count TESTS     #
######################################################


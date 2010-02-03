
############## mysql-test\t\myisam_max_sort_file_size_basic.test ################
#                                                                              #
# Variable Name: myisam_max_sort_file_size                                      #
# Scope: GLOBAL & SESSION                                                      #
# Access Type: Dynamic                                                         #
# Data Type: Numeric                                                           #
# Default Value: 1                                                             #
# Range: 1 - 65536                                                             #
#                                                                              #
#                                                                              #
# Creation Date: 2008-02-07                                                    #
# Author:  Rizwan Maredia                                                      #
#                                                                              #
# Description: Test Cases of Dynamic System Variable myisam_max_sort_file_size  #
#              that checks the behavior of this variable in the following ways #
#              * Default Value                                                 #
#              * Valid & Invalid values                                        #
#              * Scope & Access method                                         #
#              * Data Integrity                                                #
#                                                                              #
# Reference: http://dev.mysql.com/doc/refman/5.1/en/                           #
#  server-system-variables.html                                                #
#                                                                              #
################################################################################

--source include/load_sysvars.inc

###################################################################
#           START OF myisam_max_sort_file_size TESTS              #
###################################################################


#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.myisam_max_sort_file_size;
SELECT @start_global_value;

--echo '#--------------------FN_DYNVARS_094_01-------------------------#'
###################################################################
#     Display the DEFAULT value of myisam_max_sort_file_size      #
###################################################################

SET @@global.myisam_max_sort_file_size = 500000;
SET @@global.myisam_max_sort_file_size = DEFAULT;
SELECT @@global.myisam_max_sort_file_size;

--echo '#--------------------FN_DYNVARS_094_02-------------------------#'
###################################################################
#     Check the DEFAULT value of myisam_max_sort_file_size         #
###################################################################

SET @@global.myisam_max_sort_file_size = DEFAULT;
SELECT @@global.myisam_max_sort_file_size = 2147483648;

--echo '#--------------------FN_DYNVARS_094_03-------------------------#'
##################################################################################
# Change the value of myisam_max_sort_file_size to a valid value for GLOBAL Scope #
##################################################################################

SET @@global.myisam_max_sort_file_size = 0;
SELECT @@global.myisam_max_sort_file_size;
SET @@global.myisam_max_sort_file_size = 1024;
SELECT @@global.myisam_max_sort_file_size;
SET @@global.myisam_max_sort_file_size = 123456789;
SELECT @@global.myisam_max_sort_file_size;
SET @@global.myisam_max_sort_file_size = 2147483648*2;
SELECT @@global.myisam_max_sort_file_size;
SET @@global.myisam_max_sort_file_size = 2147483648*1024;
SELECT @@global.myisam_max_sort_file_size;
SELECT @@global.myisam_max_sort_file_size;
SET @@global.myisam_max_sort_file_size = 2147483648*2147483648;

--echo '#--------------------FN_DYNVARS_094_04-------------------------#'
#################################################################################
#           Check if variable can be access with session scope                  #
#################################################################################

--Error ER_GLOBAL_VARIABLE
SET @@myisam_max_sort_file_size = 2;

--Error ER_GLOBAL_VARIABLE
SET @@session.myisam_max_sort_file_size = 3;

--Error ER_GLOBAL_VARIABLE
SET @@local.myisam_max_sort_file_size = 4;



--echo '#------------------FN_DYNVARS_094_05-----------------------#'
####################################################################
# Change the value of myisam_max_sort_file_size to an invalid value #
####################################################################

SET @@global.myisam_max_sort_file_size = -1;
SELECT @@global.myisam_max_sort_file_size;
SET @@global.myisam_max_sort_file_size = -2147483648;
SELECT @@global.myisam_max_sort_file_size;
SET @@global.myisam_max_sort_file_size = -2147483649;
SELECT @@global.myisam_max_sort_file_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.myisam_max_sort_file_size = 65530.34;
SELECT @@global.myisam_max_sort_file_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.myisam_max_sort_file_size = 2147483649.56;
SELECT @@global.myisam_max_sort_file_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.myisam_max_sort_file_size = 1G;

--echo '#------------------FN_DYNVARS_094_06-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################

SET @@global.myisam_max_sort_file_size = 3000;
SELECT @@global.myisam_max_sort_file_size = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='myisam_max_sort_file_size';


--echo '#------------------FN_DYNVARS_094_07-----------------------#'
###########################################################################
#  Check if the value is present in INFORMATION_SCHEMA.SESSION_VARIABLES  #
###########################################################################

SELECT count(VARIABLE_VALUE) 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='myisam_max_sort_file_size';


--echo '#------------------FN_DYNVARS_094_08-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.myisam_max_sort_file_size = TRUE;
SELECT @@global.myisam_max_sort_file_size;
SET @@global.myisam_max_sort_file_size = FALSE;
SELECT @@global.myisam_max_sort_file_size;


--echo '#---------------------FN_DYNVARS_001_09----------------------#'
#################################################################################
#  Check if accessing variable with and without GLOBAL point to same variable   #
################################################################################# 


SET @@global.myisam_max_sort_file_size = 512;
SELECT @@myisam_max_sort_file_size = @@global.myisam_max_sort_file_size;


--echo '#---------------------FN_DYNVARS_001_10----------------------#'
##################################################################################
#   Check if myisam_max_sort_file_size can be accessed without @@ sign and scope #
##################################################################################

--Error ER_GLOBAL_VARIABLE
SET myisam_max_sort_file_size = 2048;
--Error ER_BAD_FIELD_ERROR
SELECT myisam_max_sort_file_size;

SELECT @@myisam_max_sort_file_size;

#verifying another syntax for setting value#
SET global myisam_max_sort_file_size = 64;

####################################
#     Restore initial value        #
####################################

SET @@global.myisam_max_sort_file_size = @start_global_value;
SELECT @@global.myisam_max_sort_file_size;

########################################################
#              END OF myisam_max_sort_file_size TESTS  #
########################################################


############## mysql-test\t\rpl_recovery_rank_basic.test #######################
#                                                                              #
# Variable Name: rpl_recovery_rank                                             #
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
# Description: Test Cases of Dynamic System Variable rpl_recovery_rank         #
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
#           START OF rpl_recovery_rank TESTS                      #
###################################################################


#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.rpl_recovery_rank;
SELECT @start_global_value;

--echo '#--------------------FN_DYNVARS_142_01-------------------------#'
###################################################################
#     Check the DEFAULT value of rpl_recovery_rank               #
###################################################################

SET @@global.rpl_recovery_rank = 500000;
SET @@global.rpl_recovery_rank = DEFAULT;
SELECT @@global.rpl_recovery_rank;

--echo '#--------------------FN_DYNVARS_142_02-------------------------#'
##################################################################################
# Change the value of rpl_recovery_rank to a valid value for GLOBAL Scope        #
##################################################################################

SET @@global.rpl_recovery_rank = 0;
SELECT @@global.rpl_recovery_rank;
SET @@global.rpl_recovery_rank = 1024;
SELECT @@global.rpl_recovery_rank;
SET @@global.rpl_recovery_rank = 123456789;
SELECT @@global.rpl_recovery_rank;
SET @@global.rpl_recovery_rank = 2147483648*2;
SELECT @@global.rpl_recovery_rank;
SET @@global.rpl_recovery_rank = 2147483648*1024;
SELECT @@global.rpl_recovery_rank;
SELECT @@global.rpl_recovery_rank;
SET @@global.rpl_recovery_rank = 2147483648*2147483648;
SELECT @@global.rpl_recovery_rank;


--echo '#--------------------FN_DYNVARS_142_03-------------------------#'
#################################################################################
#           Check if variable can be access with session scope                  #
#################################################################################

--Error ER_GLOBAL_VARIABLE
SET @@rpl_recovery_rank = 2;

--Error ER_GLOBAL_VARIABLE
SET @@session.rpl_recovery_rank = 3;

--Error ER_GLOBAL_VARIABLE
SET @@local.rpl_recovery_rank = 4;



--echo '#------------------FN_DYNVARS_142_04-----------------------#'
####################################################################
# Change the value of rpl_recovery_rank to an invalid value        #
####################################################################

SET @@global.rpl_recovery_rank = -1;
SELECT @@global.rpl_recovery_rank;
SET @@global.rpl_recovery_rank = -2147483648;
SELECT @@global.rpl_recovery_rank;
SET @@global.rpl_recovery_rank = -2147483649;
SELECT @@global.rpl_recovery_rank;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.rpl_recovery_rank = 65530.34;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.rpl_recovery_rank = 2147483649.56;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.rpl_recovery_rank = 1G;


--echo '#------------------FN_DYNVARS_142_05-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################

SET @@global.rpl_recovery_rank = 3000;
SELECT @@global.rpl_recovery_rank = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='rpl_recovery_rank';


--echo '#------------------FN_DYNVARS_142_06-----------------------#'
###########################################################################
#  Check if the value is present in INFORMATION_SCHEMA.SESSION_VARIABLES  #
###########################################################################

SELECT count(VARIABLE_VALUE) 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='rpl_recovery_rank';


--echo '#------------------FN_DYNVARS_142_07-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.rpl_recovery_rank = TRUE;
SELECT @@global.rpl_recovery_rank;
SET @@global.rpl_recovery_rank = FALSE;
SELECT @@global.rpl_recovery_rank;


--echo '#---------------------FN_DYNVARS_001_08----------------------#'
#################################################################################
#  Check if accessing variable with and without GLOBAL point to same variable   #
################################################################################# 


SET @@global.rpl_recovery_rank = 512;
SELECT @@rpl_recovery_rank = @@global.rpl_recovery_rank;


--echo '#---------------------FN_DYNVARS_001_09----------------------#'
##################################################################################
#   Check if rpl_recovery_rank can be accessed without @@ sign and scope         #
##################################################################################

--Error ER_GLOBAL_VARIABLE
SET rpl_recovery_rank = 2048;
--Error ER_BAD_FIELD_ERROR
SELECT rpl_recovery_rank;

SELECT @@rpl_recovery_rank;

#verifying another another syntax for setting value
SET global rpl_recovery_rank = 64;

####################################
#     Restore initial value        #
####################################

SET @@global.rpl_recovery_rank = @start_global_value;
SELECT @@global.rpl_recovery_rank;

########################################################
#              END OF rpl_recovery_rank TESTS          #
########################################################


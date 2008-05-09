################# mysql-test\t\innodb_max_purge_lag_basic.test ################
#                                                                             #
# Variable Name: innodb_max_purge_lag                                         #
# Scope: GLOBAL                                                               #
# Access Type: Dynamic                                                        #
# Data Type: Numeric                                                          #
# Default Value: 0                                                            #
# Range: 0-4294967295                                                         #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Rizwan                                                             #
#                                                                             #
#Description:Test Cases of Dynamic System Variable innodb_max_purge_lag       #
#             that checks the behavior of this variable in the following ways #
#              * Default Value                                                #
#              * Valid & Invalid values                                       #
#              * Scope & Access method                                        #
#              * Data Integrity                                               #
#                                                                             #
# Reference: http://dev.mysql.com/doc/refman/5.1/en/                          #
#  server-system-variables.html                                               #
#                                                                             #
###############################################################################

--source include/have_innodb.inc
--source include/load_sysvars.inc

######################################################################## 
#                    START OF innodb_max_purge_lag TESTS               #
######################################################################## 

################################################################################ 
#   Saving initial value of innodb_max_purge_lag in a temporary variable       #
################################################################################ 

SET @global_start_value = @@global.innodb_max_purge_lag;
SELECT @global_start_value;

--echo '#--------------------FN_DYNVARS_046_01------------------------#'
######################################################################## 
#           Display the DEFAULT value of innodb_max_purge_lag          #
######################################################################## 

SET @@global.innodb_max_purge_lag = 0;
SET @@global.innodb_max_purge_lag = DEFAULT;
SELECT @@global.innodb_max_purge_lag;

--echo '#---------------------FN_DYNVARS_046_02-------------------------#'
############################################################################
#   Check if innodb_max_purge_lag can be accessed with and without @@ sign #
############################################################################

--Error ER_GLOBAL_VARIABLE
SET innodb_max_purge_lag = 1;
SELECT @@innodb_max_purge_lag;

--Error ER_UNKNOWN_TABLE
SELECT local.innodb_max_purge_lag;

SET global innodb_max_purge_lag = 0;
SELECT @@global.innodb_max_purge_lag;

--echo '#--------------------FN_DYNVARS_046_03------------------------#'
########################################################################## 
#      change the value of innodb_max_purge_lag to a valid value         #
########################################################################## 


SET @@global.innodb_max_purge_lag = 0;
SELECT @@global.innodb_max_purge_lag;

SET @@global.innodb_max_purge_lag = 1;
SELECT @@global.innodb_max_purge_lag;
SET @@global.innodb_max_purge_lag = 4294967295;
SELECT @@global.innodb_max_purge_lag;

--echo '#--------------------FN_DYNVARS_046_04-------------------------#'
########################################################################### 
#       Change the value of innodb_max_purge_lag to invalid value         #
########################################################################### 

SET @@global.innodb_max_purge_lag = -1;
SELECT @@global.innodb_max_purge_lag;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.innodb_max_purge_lag = "T";
SELECT @@global.innodb_max_purge_lag;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.innodb_max_purge_lag = "Y";
SELECT @@global.innodb_max_purge_lag;


SET @@global.innodb_max_purge_lag = 1001;
SELECT @@global.innodb_max_purge_lag;

--echo '#----------------------FN_DYNVARS_046_05------------------------#'
######################################################################### 
#     Check if the value in GLOBAL Table matches value in variable      #
#########################################################################

SELECT @@global.innodb_max_purge_lag =
 VARIABLE_VALUE FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES
  WHERE VARIABLE_NAME='innodb_max_purge_lag';
SELECT @@global.innodb_max_purge_lag;
SELECT VARIABLE_VALUE FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES
 WHERE VARIABLE_NAME='innodb_max_purge_lag';




--echo '#---------------------FN_DYNVARS_046_06-------------------------#'
################################################################### 
#        Check if ON and OFF values can be used on variable       #
################################################################### 

--ERROR ER_WRONG_TYPE_FOR_VAR
SET @@global.innodb_max_purge_lag = OFF;
SELECT @@global.innodb_max_purge_lag;

--ERROR ER_WRONG_TYPE_FOR_VAR
SET @@global.innodb_max_purge_lag = ON;
SELECT @@global.innodb_max_purge_lag;

--echo '#---------------------FN_DYNVARS_046_07----------------------#'
################################################################### 
#      Check if TRUE and FALSE values can be used on variable     #
################################################################### 


SET @@global.innodb_max_purge_lag = TRUE;
SELECT @@global.innodb_max_purge_lag;
SET @@global.innodb_max_purge_lag = FALSE;
SELECT @@global.innodb_max_purge_lag;

##############################  
#   Restore initial value    #
##############################


SET @@global.innodb_max_purge_lag = @global_start_value;
SELECT @@global.innodb_max_purge_lag;

###############################################################
#                    END OF innodb_max_purge_lag TESTS        #
############################################################### 

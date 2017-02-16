############### mysql-test\t\delayed_insert_limit_basic.test ###################
#                                                                              #
# Variable Name: delayed_insert_limit                                          #
# Scope: GLOBAL                                                                #
# Access Type: Dynamic                                                         #
# Data Type: Numeric                                                           #
# Default Value: 100                                                           #
# Range: 1 - 4294967295                                                        #
#                                                                              #
#                                                                              #
# Creation Date: 2008-02-07                                                    #
# Author:  Salman Rawala                                                       #
#                                                                              #
# Description: Test Cases of Dynamic System Variable "delayed_insert_limit"    #
#              that checks behavior of this variable in the following ways     #
#              * Default Value                                                 #
#              * Valid & Invalid values                                        #
#              * Scope & Access method                                         #
#              * Data Integrity                                                #
#                                                                              #
# Reference: http://dev.mysql.com/doc/refman/5.1/en/                           #
#        server-system-variables.html#option_mysqld_delayed_insert_limit       #
#                                                                              #
################################################################################

--source include/load_sysvars.inc

#################################################################
#           START OF delayed_insert_limit TESTS                 #
#################################################################

#########################################################################
# Saving initial value of delayed_insert_limit in a temporary variable  #
#########################################################################

SET @start_value = @@global.delayed_insert_limit;
SELECT @start_value;

--echo '#--------------------FN_DYNVARS_024_01------------------------#'
######################################################################### 
#              Display the DEFAULT value of delayed_insert_limit        #
######################################################################### 

SET @@global.delayed_insert_limit = 100;
SET @@global.delayed_insert_limit = DEFAULT;
SELECT @@global.delayed_insert_limit;


--echo '#---------------------FN_DYNVARS_024_02-------------------------#'
############################################### 
#     Verify default value of variable        #
############################################### 

SET @@global.delayed_insert_limit = @start_value;
SELECT @@global.delayed_insert_limit = 100;


--echo '#--------------------FN_DYNVARS_024_03------------------------#'
#########################################################################
#        Change the value of delayed_insert_limit to a valid value      #
#########################################################################

SET @@global.delayed_insert_limit = 10000;
SELECT @@global.delayed_insert_limit;
SET @@global.delayed_insert_limit = 4294967295;
SELECT @@global.delayed_insert_limit;
SET @@global.delayed_insert_limit = 1;
SELECT @@global.delayed_insert_limit;


--echo '#--------------------FN_DYNVARS_024_04-------------------------#'
############################################################################
#         Change the value of delayed_insert_limit to invalid value        #
############################################################################ 

SET @@global.delayed_insert_limit = 0;
SELECT @@global.delayed_insert_limit;
SET @@global.delayed_insert_limit = -1024;
SELECT @@global.delayed_insert_limit;
SET @@global.delayed_insert_limit = 42949672950;
SELECT @@global.delayed_insert_limit;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.delayed_insert_limit = 429496729.5;
SELECT @@global.delayed_insert_limit;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.delayed_insert_limit = ON;
SELECT @@global.delayed_insert_limit;



--echo '#-------------------FN_DYNVARS_024_05----------------------------#'
############################################################################
#       Test if accessing session delayed_insert_limit gives error         #
############################################################################

--Error ER_GLOBAL_VARIABLE
SET @@session.delayed_insert_limit = 0;
--Error ER_INCORRECT_GLOBAL_LOCAL_VAR
SELECT @@Session.delayed_insert_limit;


--echo '#----------------------FN_DYNVARS_024_06------------------------#'
############################################################################## 
# Check if the value in GLOBAL & SESSION Tables matches values in variable   #
##############################################################################

SELECT @@global.delayed_insert_limit =
 VARIABLE_VALUE FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES
  WHERE VARIABLE_NAME='delayed_insert_limit';

SELECT @@delayed_insert_limit =
 VARIABLE_VALUE FROM INFORMATION_SCHEMA.SESSION_VARIABLES
  WHERE VARIABLE_NAME='delayed_insert_limit';


--echo '#---------------------FN_DYNVARS_024_07----------------------#'
################################################################### 
#      Check if TRUE and FALSE values can be used on variable     #
################################################################### 

SET @@global.delayed_insert_limit = TRUE;
SELECT @@global.delayed_insert_limit;
SET @@global.delayed_insert_limit = FALSE;
SELECT @@global.delayed_insert_limit;

--echo '#---------------------FN_DYNVARS_024_08----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@global.delayed_insert_limit = 1;
SELECT @@delayed_insert_limit = @@global.delayed_insert_limit;


--echo '#---------------------FN_DYNVARS_024_09----------------------#'
##############################################################################
#   Check if delayed_insert_limit can be accessed with and without @@ sign   #
##############################################################################

--Error ER_GLOBAL_VARIABLE
SET delayed_insert_limit = 1;
SELECT @@delayed_insert_limit;
--Error ER_PARSE_ERROR
SET local.delayed_insert_limit = 1;
--Error ER_UNKNOWN_TABLE
SELECT local.delayed_insert_limit;
--Error ER_PARSE_ERROR
SET global.delayed_insert_limit = 1;
--Error ER_UNKNOWN_TABLE
SELECT global.delayed_insert_limit;
--Error ER_BAD_FIELD_ERROR
SELECT delayed_insert_limit = @@session.delayed_insert_limit;

##############################
#   Restore initial value    #
##############################

SET @@global.delayed_insert_limit = @start_value;
SELECT @@global.delayed_insert_limit;


###########################################################
#           END OF delayed_insert_limit TESTS             #
###########################################################


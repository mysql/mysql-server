##################### mysql-test\t\log_warnings_basic.test ######################
#                                                                               #
# Variable Name: log_warnings                                                   #
# Scope: GLOBAL                                                                 #
# Access Type: Dynamic                                                          #
# Data Type: numeric                                                            #
# Default Value: 1                                                              #
# Range: 0 - 4294967295/18446744073709551615                                    #
#                                                                               #
#                                                                               #
# Creation Date: 2008-02-07                                                     #
# Author:  Salman                                                               #
#                                                                               #
# Description: Test Cases of Dynamic System Variable log_warnings               #
#              that checks the behavior of this variable in the following ways  #
#              * Default Value                                                  #
#              * Valid & Invalid values                                         #
#              * Scope & Access method                                          #
#              * Data Integrity                                                 #
#                                                                               #
# Modified:    Pavan Naik, 2015-11-24                                           #
#              Added boundary value test cases according to the type of the     #
#              server binary.                                                   #
#                                                                               #
# Reference: http://dev.mysql.com/doc/refman/5.1/en/                            #
#            server-system-variables.html                                       #
#                                                                               #
#################################################################################

--source include/load_sysvars.inc

############################################################
#           START OF log_warnings TESTS                    #
############################################################


#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.log_warnings;
SELECT @start_global_value;


--echo '#--------------------FN_DYNVARS_067_01-------------------------#'
########################################################################
#     Display the DEFAULT value of log_warnings            #
########################################################################

SET @@global.log_warnings = 100;
SET @@global.log_warnings = DEFAULT;
SELECT @@global.log_warnings;


--echo '#--------------------FN_DYNVARS_067_02-------------------------#'
########################################################################
#     Check the DEFAULT value of log_warnings              #
########################################################################

SET @@global.log_warnings = DEFAULT;
SELECT @@global.log_warnings = 2;


--echo '#--------------------FN_DYNVARS_067_03-------------------------#'
######################################################################
# Change the value of log_warnings to a valid value for GLOBAL Scope #
######################################################################

SET @@global.log_warnings = 0;
SELECT @@global.log_warnings;
SET @@global.log_warnings = 1;
SELECT @@global.log_warnings;
SET @@global.log_warnings = 60020;
SELECT @@global.log_warnings;
SET @@global.log_warnings = 65535;
SELECT @@global.log_warnings;
SET @@global.log_warnings = 65536;
SELECT @@global.log_warnings;

SET @@global.log_warnings = 4294967295;
SELECT @@global.log_warnings;

# Test cases for 64-bit machines
if($mach64)
{
  SET @@global.log_warnings = 18446744073709551615;
  SELECT @@global.log_warnings;
}

--echo '#------------------FN_DYNVARS_067_04-----------------------#'
########################################################
# Change the value of log_warnings to an invalid value #
########################################################

SET @@global.log_warnings = -1;
SELECT @@global.log_warnings;
SET @@global.log_warnings = -1024;
SELECT @@global.log_warnings;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.log_warnings = 65530.34;
SELECT @@global.log_warnings;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.log_warnings = test;
SELECT @@global.log_warnings;

if($mach32)
{
  SET @@global.log_warnings = 4294967296;
  SELECT @@global.log_warnings;

  SET @@global.log_warnings = 42949672950;
  SELECT @@global.log_warnings;
}

if($mach64)
{
  --error ER_WRONG_TYPE_FOR_VAR
  SET @@global.log_warnings = 18446744073709551616;
  SELECT @@global.log_warnings;
}

--echo '#------------------FN_DYNVARS_067_05-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################

--disable_warnings
SELECT VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='log_warnings'
into @tmp;
--enable_warnings

SELECT @@global.log_warnings = @tmp;

--echo '#------------------FN_DYNVARS_067_06-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.log_warnings = TRUE;
SELECT @@global.log_warnings;
SET @@global.log_warnings = FALSE;
SELECT @@global.log_warnings;


--echo '#---------------------FN_DYNVARS_067_07----------------------#'
#################################################################################
#  Check if accessing variable with and without GLOBAL point to same variable   #
#################################################################################

SET @@global.log_warnings = 10;
SELECT @@log_warnings = @@global.log_warnings;


####################################
#     Restore initial value        #
####################################

SET @@global.log_warnings = @start_global_value;
SELECT @@global.log_warnings;


#############################################################
#                 END OF log_warnings TESTS     #
#############################################################


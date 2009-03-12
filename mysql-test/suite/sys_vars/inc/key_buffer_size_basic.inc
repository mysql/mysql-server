############## mysql-test\t\key_buffer_size_basic.test ########################
#                                                                             #
# Variable Name: key_buffer_size                                              #
# Scope: GLOBAL                                                               #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value: 131072                                                       #
# Range:8-4294967295                                                          #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable key_buffer_size          #
#              that checks the behavior of this variable in the following ways#
#              * Default Value                                                #
#              * Valid & Invalid values                                       #
#              * Scope & Access method                                        #
#              * Data Integrity                                               #
# Modified:    Horst Hunger 2008-11-27                                        #
#              Due to OS depending values the check has been changed          #
#              from concrete values to ranges.                                #
#                                                                             #
# Reference:                                                                  #
#  http://dev.mysql.com/doc/refman/5.1/en/server-system-variables.html        #
#                                                                             #
###############################################################################

--source include/load_sysvars.inc

let $minimum_range= BETWEEN 8 AND 36;
########################################################################
#                    START OF key_buffer_size TESTS                    #
########################################################################

--disable_warnings
########################################################################
# Saving initial value of key_buffer_size in a temporary variable      #
########################################################################

SET @start_value = @@global.key_buffer_size;

--echo '#--------------------FN_DYNVARS_055_01------------------------#'
########################################################################
#              Display the DEFAULT value of key_buffer_size            #
########################################################################

SET @@global.key_buffer_size = 99;
--Error ER_NO_DEFAULT
SET @@global.key_buffer_size = DEFAULT;
eval SELECT @@global.key_buffer_size $minimum_range;

--echo '#---------------------FN_DYNVARS_055_02-------------------------#'
###############################################
#     Verify default value of variable        #
###############################################

SET @@global.key_buffer_size = @start_value;
SELECT @@global.key_buffer_size = @start_value;

--echo '#--------------------FN_DYNVARS_055_03------------------------#'
########################################################################
#        Change the value of key_buffer_size to a valid value          #
########################################################################

SET @@global.key_buffer_size = 8 ;
eval SELECT @@global.key_buffer_size $minimum_range;
--disable_warnings
# The next subtest is set to comment because it will
# be tried to allocate the buffer instantly which
# might lead to a message about missing resources,
# excessive runtime etc. especially if the main memory is less than 4GB.
#SET @@global.key_buffer_size = 4294967295;
--enable_warnings
#SELECT @@global.key_buffer_size;
SET @@global.key_buffer_size = 1800;
eval SELECT @@global.key_buffer_size $minimum_range;
SET @@global.key_buffer_size = 65535;
SELECT @@global.key_buffer_size;

--echo '#--------------------FN_DYNVARS_055_04-------------------------#'
###########################################################################
#         Change the value of key_buffer_size to invalid value            #
###########################################################################

# The next subtests are set to comment because they will
# be tried to allocate the buffer instantly which
# might lead to a message about missing resources,
# excessive runtime etc. especially if the main memory is less than 4GB.
# The negative values lead to very big numbers, e.g. -1 to
# a size of 18446744073709551615 bytes with a 32bit binary.
# See Bug #42103.
#SET @@global.key_buffer_size = -1;
#SELECT @@global.key_buffer_size;
#SET @@global.key_buffer_size = 100000000000;
#SELECT @@global.key_buffer_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.key_buffer_size = 10000.01;
SELECT @@global.key_buffer_size;
#SET @@global.key_buffer_size = -1024;
#SELECT @@global.key_buffer_size;
SET @@global.key_buffer_size = 4;
eval SELECT @@global.key_buffer_size $minimum_range;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.key_buffer_size = ON;
eval SELECT @@global.key_buffer_size $minimum_range;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.key_buffer_size = 'test';
eval SELECT @@global.key_buffer_size $minimum_range;

--echo '#-------------------FN_DYNVARS_055_05----------------------------#'
###########################################################################
#       Test if accessing session key_buffer_size gives error             #
###########################################################################

--Error ER_GLOBAL_VARIABLE
SET @@session.key_buffer_size = 0;
eval SELECT @@global.key_buffer_size $minimum_range;

--echo '#----------------------FN_DYNVARS_055_06------------------------#'
##############################################################################
# Check if the value in GLOBAL & SESSION Tables matches values in variable   #
##############################################################################

SELECT @@global.key_buffer_size = VARIABLE_VALUE
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES
WHERE VARIABLE_NAME='key_buffer_size';

SELECT @@key_buffer_size = VARIABLE_VALUE
FROM INFORMATION_SCHEMA.SESSION_VARIABLES
WHERE VARIABLE_NAME='key_buffer_size';

--echo '#---------------------FN_DYNVARS_055_07----------------------#'
###################################################################
#      Check if TRUE and FALSE values can be used on variable     #
###################################################################

# The following assignment are accepted, but should be rejected
# like the value ON (see above).
# See Bug #42104
SET @@global.key_buffer_size = TRUE;
eval SELECT @@global.key_buffer_size $minimum_range;
SET @@global.key_buffer_size = FALSE;
eval SELECT @@global.key_buffer_size $minimum_range;

--echo '#---------------------FN_DYNVARS_055_08----------------------#'
#####################################################################
#  Check if accessing variable with SESSION,LOCAL and without SCOPE #
#  points to same session variable                                  #
#####################################################################

# due to differences in contents of the warnings
--disable_warnings
SET @@global.key_buffer_size = 8 ;
SELECT @@key_buffer_size = @@global.key_buffer_size;
--enable_warnings

--echo '#---------------------FN_DYNVARS_055_09----------------------#'
##########################################################################
#   Check if key_buffer_size can be accessed with and without @@ sign    #
##########################################################################

--Error ER_GLOBAL_VARIABLE
SET key_buffer_size = 8 ;
eval SELECT @@global.key_buffer_size $minimum_range;
--Error ER_PARSE_ERROR
SET local.key_buffer_size = 10;
--Error ER_UNKNOWN_TABLE
SELECT local.key_buffer_size;
--Error ER_PARSE_ERROR
SET global.key_buffer_size = 10;
--Error ER_UNKNOWN_TABLE
SELECT global.key_buffer_size;
--Error ER_BAD_FIELD_ERROR
SELECT key_buffer_size = @@session.key_buffer_size;

##############################
#   Restore initial value    #
##############################

SET @@global.key_buffer_size = @start_value;

--enable_warnings
#######################################################################
#              END OF key_buffer_size TESTS                           #
#######################################################################


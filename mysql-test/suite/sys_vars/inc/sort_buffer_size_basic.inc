############## mysql-test\t\sort_buffer_size_basic.test #######################
#                                                                             #
# Variable Name: sort_buffer_size                                             #
# Scope: GLOBAL | SESSION                                                     #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value: 2097144                                                      #
# Range:max value:4294967295                                                  #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-07                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable sort_buffer_size         #
#              that checks the behavior of this variable in the following ways#
#              * Default Value                                                #
#              * Valid & Invalid values                                       #
#              * Scope & Access method                                        #
#              * Data Integrity                                               #
# Modified:    Horst Hunger, 2008-11-28                                       #
#              disabled warnings as they contain the values of this variable, #
#              modified the check of the value only checking a range.         #
#                                                                             #
# Reference:                                                                  #
#  http://dev.mysql.com/doc/refman/5.1/en/server-system-variables.html        #
#                                                                             #
###############################################################################

--source include/load_sysvars.inc

########################################################################


######################################################################
#           START OF sort_buffer_size TESTS                          #
######################################################################


#############################################################
#                 Save initial value                        #
#############################################################

# due to differences when running on Windows (bug filed)
--source include/not_windows.inc

--disable_warnings

SET @start_global_value = @@global.sort_buffer_size;
SET @start_session_value = @@session.sort_buffer_size;

--echo '#--------------------FN_DYNVARS_151_01-------------------------#'
######################################################################
#     Display the DEFAULT value of sort_buffer_size                  #
######################################################################

SET @@global.sort_buffer_size = 1000;
SET @@global.sort_buffer_size = DEFAULT;
SELECT @@global.sort_buffer_size >= 2097116 AND @@global.sort_buffer_size < 3000000;

SET @@session.sort_buffer_size = 2000;
SET @@session.sort_buffer_size = DEFAULT;
SELECT @@session.sort_buffer_size >= 2097116 AND @@session.sort_buffer_size < 3000000;


--echo '#--------------------FN_DYNVARS_151_02-------------------------#'
######################################################################
#     Check the DEFAULT value of sort_buffer_size                    #
######################################################################

SET @@global.sort_buffer_size = DEFAULT;
SELECT @@global.sort_buffer_size >= 2097116 AND @@global.sort_buffer_size < 3000000;

SET @@session.sort_buffer_size = DEFAULT;
SELECT @@session.sort_buffer_size >= 2097116 AND @@session.sort_buffer_size < 3000000;


--echo '#--------------------FN_DYNVARS_151_03-------------------------#'
################################################################################
# Change the value of sort_buffer_size to a valid value for GLOBAL Scope       #
################################################################################

SET @@global.sort_buffer_size = 32776;
SELECT @@global.sort_buffer_size >= 32776 AND @@global.sort_buffer_size < 33000;
SET @@global.sort_buffer_size = 32777;
SELECT @@global.sort_buffer_size >= 32776 AND @@global.sort_buffer_size < 33000;
SET @@global.sort_buffer_size = 4294967295;
SELECT @@global.sort_buffer_size;
SET @@global.sort_buffer_size = 4294967294;
SELECT @@global.sort_buffer_size;

--echo '#--------------------FN_DYNVARS_151_04-------------------------#'
###################################################################################
# Change the value of sort_buffer_size to a valid value for SESSION Scope         #
###################################################################################
 
SET @@session.sort_buffer_size = 32776;
SELECT @@session.sort_buffer_size >= 32776 AND @@session.sort_buffer_size < 33000;
SET @@session.sort_buffer_size = 32777;
SELECT @@session.sort_buffer_size >= 32776 AND @@session.sort_buffer_size < 33000;
SET @@session.sort_buffer_size = 4294967295;
SELECT @@session.sort_buffer_size;
SET @@session.sort_buffer_size = 4294967294;
SELECT @@session.sort_buffer_size;


--echo '#------------------FN_DYNVARS_151_05-----------------------#'
##################################################################
# Change the value of sort_buffer_size to an invalid value       #
##################################################################

SET @@global.sort_buffer_size = 32775;
SELECT @@global.sort_buffer_size >= 32776 AND @@global.sort_buffer_size < 33000;
SET @@global.sort_buffer_size = -1024;
SELECT @@global.sort_buffer_size >= 32776 AND @@global.sort_buffer_size < 33000;
SET @@global.sort_buffer_size = 4294967296;
SELECT @@global.sort_buffer_size;
--Error ER_PARSE_ERROR
SET @@global.sort_buffer_size = 65530.34.;
SELECT @@global.sort_buffer_size;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.sort_buffer_size = test;
SELECT @@global.sort_buffer_size;

SET @@session.sort_buffer_size = 32775;
SELECT @@session.sort_buffer_size >= 32776 AND @@session.sort_buffer_size < 33000;
SET @@session.sort_buffer_size = -2;
SELECT @@session.sort_buffer_size >= 32776 AND @@session.sort_buffer_size < 33000;
--Error ER_PARSE_ERROR
SET @@session.sort_buffer_size = 65530.34.;
SET @@session.sort_buffer_size = 4294967296;
SELECT @@session.sort_buffer_size;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.sort_buffer_size = test;

--echo '#------------------FN_DYNVARS_151_06-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################


SELECT @@global.sort_buffer_size = VARIABLE_VALUE FROM 
       INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE VARIABLE_NAME='sort_buffer_size';

--echo '#------------------FN_DYNVARS_151_07-----------------------#'
####################################################################
#  Check if the value in SESSION Table matches value in variable   #
####################################################################

SELECT @@session.sort_buffer_size = VARIABLE_VALUE FROM
       INFORMATION_SCHEMA.SESSION_VARIABLES WHERE VARIABLE_NAME='sort_buffer_size';


--echo '#------------------FN_DYNVARS_151_08-----------------------#'
####################################################################
#     Check if TRUE and FALSE values can be used on variable       #
####################################################################

SET @@global.sort_buffer_size = TRUE;
SELECT @@global.sort_buffer_size >= 32776 AND @@global.sort_buffer_size < 33000;
SET @@global.sort_buffer_size = FALSE;
SELECT @@global.sort_buffer_size >= 32776 AND @@global.sort_buffer_size < 33000;

--echo '#---------------------FN_DYNVARS_151_09----------------------#'
#################################################################################### 
#  Check if accessing variable with and without GLOBAL point to same variable      #
#################################################################################### 

SET @@global.sort_buffer_size = 9000;
SELECT @@sort_buffer_size = @@global.sort_buffer_size;

--echo '#---------------------FN_DYNVARS_151_10----------------------#'
###################################################################################################
# Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable#
###################################################################################################

SET @@sort_buffer_size = 9000;
SELECT @@sort_buffer_size = @@local.sort_buffer_size;
SELECT @@local.sort_buffer_size = @@session.sort_buffer_size;


--echo '#---------------------FN_DYNVARS_151_11----------------------#'
###################################################################################  
#   Check if sort_buffer_size can be accessed with and without @@ sign            #
###################################################################################

SET sort_buffer_size = 9100;
SELECT @@sort_buffer_size >= 32776 AND @@sort_buffer_size < 33000;
--Error ER_UNKNOWN_TABLE
SELECT local.sort_buffer_size;
--Error ER_UNKNOWN_TABLE
SELECT session.sort_buffer_size;
--Error ER_BAD_FIELD_ERROR
SELECT sort_buffer_size = @@session.sort_buffer_size;


####################################
#     Restore initial value        #
####################################

SET @@global.sort_buffer_size = @start_global_value;
SET @@session.sort_buffer_size = @start_session_value;

--enable_warnings

#############################################################
#                 END OF sort_buffer_size TESTS             #
#############################################################


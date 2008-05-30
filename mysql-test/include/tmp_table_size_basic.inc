############## mysql-test\t\tmp_table_size_basic.test #########################
#                                                                             #
# Variable Name: tmp_table_size                                               #
# Scope: GLOBAL | SESSION                                                     #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value: 33554432                                                     #
# Range: 1024-4294967295                                                      #
#                                                                             #
#                                                                             #
# Creation Date: 2008-02-13                                                   #
# Author:  Salman                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable tmp_table_size           #
#              that checks the behavior of this variable in the following ways#
#              * Default Value                                                #
#              * Valid & Invalid values                                       #
#              * Scope & Access method                                        #
#              * Data Integrity                                               #
#                                                                             #
# Reference: http://dev.mysql.com/doc/refman/5.1/en/                          #
#  server-system-variables.html#option_mysqld_tmp_table_size                  #
#                                                                             #
###############################################################################

--source include/load_sysvars.inc

##############################################################
#           START OF tmp_table_size TESTS                    #
##############################################################


#############################################################
#                 Save initial value                        #
#############################################################

SET @start_global_value = @@global.tmp_table_size;
SELECT @start_global_value;
SET @start_session_value = @@session.tmp_table_size;
SELECT @start_session_value;

--echo 'Bug# 34876: This variable has invalid default value as compared to documentation';

--echo '#--------------------FN_DYNVARS_005_01-------------------------#'
##############################################################
#     Display the DEFAULT value of tmp_table_size            #
##############################################################

SET @@global.tmp_table_size = 100;
SET @@global.tmp_table_size = DEFAULT;
SELECT @@global.tmp_table_size;

--echo 'Bug# 34876: This variable has invalid default value as compared to documentation';
SET @@session.tmp_table_size = 200;
SET @@session.tmp_table_size = DEFAULT;
SELECT @@session.tmp_table_size;
--echo 'Bug# 34876: This variable has invalid default value as compared to documentation';


--echo '#--------------------FN_DYNVARS_005_02-------------------------#'
########################################################################
#     Check the DEFAULT value of tmp_table_size              #
########################################################################

SET @@global.tmp_table_size = DEFAULT;
SELECT @@global.tmp_table_size = 33554432;

SET @@session.tmp_table_size = DEFAULT;
SELECT @@session.tmp_table_size = 33554432;

--echo 'Bug# 34876: This variable has invalid default value as compared to documentation';

--echo '#--------------------FN_DYNVARS_005_03-------------------------#'
########################################################################
# Change the value of tmp_table_size to a valid value for GLOBAL Scope #
########################################################################

SET @@global.tmp_table_size = 1024;
SELECT @@global.tmp_table_size;
SET @@global.tmp_table_size = 60020;
SELECT @@global.tmp_table_size;
SET @@global.tmp_table_size = 4294967295;
SELECT @@global.tmp_table_size;


--echo '#--------------------FN_DYNVARS_005_04-------------------------#'
#########################################################################
# Change the value of tmp_table_size to a valid value for SESSION Scope #
#########################################################################
 
SET @@session.tmp_table_size = 1024;
SELECT @@session.tmp_table_size;

SET @@session.tmp_table_size =4294967295;
SELECT @@session.tmp_table_size;
SET @@session.tmp_table_size = 65535;
SELECT @@session.tmp_table_size;


--echo '#------------------FN_DYNVARS_005_05-----------------------#'
##########################################################
# Change the value of tmp_table_size to an invalid value #
##########################################################

SET @@global.tmp_table_size = 0;
SELECT @@global.tmp_table_size;

SET @@global.tmp_table_size = -1024;
SELECT @@global.tmp_table_size;

SET @@global.tmp_table_size = 1000;
SELECT @@global.tmp_table_size;

--echo 'Bug # 34837: Errors are not coming on assigning invalid values to variable';


-- Error ER_WRONG_TYPE_FOR_VAR
SET @@global.tmp_table_size = ON;


-- Error ER_WRONG_TYPE_FOR_VAR
SET @@global.tmp_table_size = OFF;


SET @@global.tmp_table_size = True;
SELECT @@global.tmp_table_size;

SET @@global.tmp_table_size = False;
SELECT @@global.tmp_table_size;
--echo 'Bug:Error should be shown that variable is numeric and can not assigned boolean value';


-- Error ER_WRONG_TYPE_FOR_VAR
SET @@global.tmp_table_size = 65530.34;

-- Error ER_WRONG_TYPE_FOR_VAR
SET @@global.tmp_table_size ="Test";


-- Error ER_WRONG_TYPE_FOR_VAR
SET @@session.tmp_table_size = ON;


-- Error ER_WRONG_TYPE_FOR_VAR
SET @@session.tmp_table_size = OFF;

SET @@session.tmp_table_size = True;
SELECT @@session.tmp_table_size;

SET @@session.tmp_table_size = False;
SELECT @@session.tmp_table_size;

-- Error ER_WRONG_TYPE_FOR_VAR
SET @@session.tmp_table_size = "Test";

SET @@session.tmp_table_size = 12345678901;
SELECT @@session.tmp_table_size;


--echo '#------------------FN_DYNVARS_005_06-----------------------#'
####################################################################
#   Check if the value in GLOBAL Table matches value in variable   #
####################################################################


SELECT @@global.tmp_table_size = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='tmp_table_size';

--echo '#------------------FN_DYNVARS_005_07-----------------------#'
####################################################################
#  Check if the value in SESSION Table matches value in variable   #
####################################################################

SELECT @@session.tmp_table_size = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='tmp_table_size';




--echo '#---------------------FN_DYNVARS_001_09----------------------#'
########################################################################
#  Check if global and session variables are independent of each other #
########################################################################

SET @@global.tmp_table_size = 1024;
SET @@tmp_table_size = 4294967295;
SELECT @@tmp_table_size = @@global.tmp_table_size;

--echo '#---------------------FN_DYNVARS_001_10----------------------#'
##################################################################
#    Check if accessing variable with SESSION,LOCAL and without  #
#    SCOPE points to same session variable                       #
##################################################################

SET @@tmp_table_size = 100;
SELECT @@tmp_table_size = @@local.tmp_table_size;
SELECT @@local.tmp_table_size = @@session.tmp_table_size;


--echo '#---------------------FN_DYNVARS_001_11----------------------#'
#########################################################################
#   Check if tmp_table_size can be accessed with and without @@ sign    #
#########################################################################

SET tmp_table_size = 1027;
SELECT @@tmp_table_size;

--Error ER_UNKNOWN_TABLE
SELECT local.tmp_table_size;

--Error ER_UNKNOWN_TABLE
SELECT global.tmp_table_size;

--Error ER_BAD_FIELD_ERROR
SELECT tmp_table_size = @@session.tmp_table_size;


####################################
#     Restore initial value        #
####################################

SET @@global.tmp_table_size = @start_global_value;
SELECT @@global.tmp_table_size;
SET @@session.tmp_table_size = @start_session_value;
SELECT @@session.tmp_table_size;


###################################################
#                 END OF tmp_table_size TESTS     #
###################################################


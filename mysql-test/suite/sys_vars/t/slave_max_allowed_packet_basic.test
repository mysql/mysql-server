############## mysql-test\t\slave_max_allowed_packet_basic.test ##################
#                                                                             #
# Variable Name: slave_max_allowed_packet                                     #
# Scope: GLOBAL                                                               #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
# Default Value:1073741824                                                    #
# Range: 1024 - 1073741824                                                    #
#                                                                             #
#                                                                             #
#                                                                             #
# Description: Test Cases of Dynamic System Variable slave_max_allowed_packet #
#              that checks the behavior of this variable in the following ways#
#              * Default Value                                                #
#              * Valid & Invalid values                                       #
#              * Scope & Access method                                        #
#              * Data Integrity                                               #
#                                                                             #
###############################################################################

--source include/load_sysvars.inc

######################################################################## 
#              START OF slave_max_allowed_packet TESTS                 #
######################################################################## 


########################################################################### 
# Saving initial value of slave_max_allowed_packet in a temporary variable#
########################################################################### 

SET @start_value = @@global.slave_max_allowed_packet;
SELECT @start_value;


--echo '#--------------------FN_DYNVARS_072_01------------------------#'
######################################################################## 
#           Display the DEFAULT value of slave_max_allowed_packet      #
######################################################################## 

SET @@global.slave_max_allowed_packet = 5000;
SET @@global.slave_max_allowed_packet = DEFAULT;
SELECT @@global.slave_max_allowed_packet;


--echo '#---------------------FN_DYNVARS_072_02-------------------------#'
############################################### 
#     Verify default value of variable        #
############################################### 

SET @@global.slave_max_allowed_packet = @start_value;
SELECT @@global.slave_max_allowed_packet = 1073741824; 
--echo 'Bug# 34876: Incorrect Default Value is assigned to variable';

--echo '#--------------------FN_DYNVARS_072_03------------------------#'
######################################################################## 
#    Change the value of slave_max_allowed_packet to a valid value     #
######################################################################## 

SET @@global.slave_max_allowed_packet = 1024;
SELECT @@global.slave_max_allowed_packet;
SET @@global.slave_max_allowed_packet = 1073741824;
SELECT @@global.slave_max_allowed_packet;
SET @@global.slave_max_allowed_packet = 1073741824;
SELECT @@global.slave_max_allowed_packet;
SET @@global.slave_max_allowed_packet = 1025;
SELECT @@global.slave_max_allowed_packet;
SET @@global.slave_max_allowed_packet = 65535;
SELECT @@global.slave_max_allowed_packet;
--echo 'Bug# 34877: Invalid Values are coming in variable on assigning valid values';


--echo '#--------------------FN_DYNVARS_072_04-------------------------#'
########################################################################### 
#      Change the value of slave_max_allowed_packet to invalid value         #
########################################################################### 

SET @@global.slave_max_allowed_packet = -1;
SELECT @@global.slave_max_allowed_packet;
SET @@global.slave_max_allowed_packet = 100000000000;
SELECT @@global.slave_max_allowed_packet;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.slave_max_allowed_packet = 10000.01;
SELECT @@global.slave_max_allowed_packet;
SET @@global.slave_max_allowed_packet = -1024;
SELECT @@global.slave_max_allowed_packet;
SET @@global.slave_max_allowed_packet = 4294967296;
SELECT @@global.slave_max_allowed_packet;
SET @@global.slave_max_allowed_packet = 1023;
SELECT @@global.slave_max_allowed_packet;

--echo 'Bug # 34837: Errors are not coming on assigning invalid values to variable';

--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.slave_max_allowed_packet = ON;
SELECT @@global.slave_max_allowed_packet;
--Error ER_WRONG_TYPE_FOR_VAR
SET @@global.slave_max_allowed_packet = 'test';
SELECT @@global.slave_max_allowed_packet;


--echo '#-------------------FN_DYNVARS_072_05----------------------------#'
########################################################################### 
#       Test if accessing session slave_max_allowed_packet gives error       #
########################################################################### 

--Error ER_GLOBAL_VARIABLE
SET @@session.slave_max_allowed_packet = 4096;
--Error ER_INCORRECT_GLOBAL_LOCAL_VAR
SELECT @@session.slave_max_allowed_packet;


--echo '#----------------------FN_DYNVARS_072_06------------------------#'
############################################################################## 
# Check if the value in GLOBAL & SESSION Tables matches values in variable   #
##############################################################################

SELECT @@global.slave_max_allowed_packet = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES 
WHERE VARIABLE_NAME='slave_max_allowed_packet';

SELECT @@slave_max_allowed_packet = VARIABLE_VALUE 
FROM INFORMATION_SCHEMA.SESSION_VARIABLES 
WHERE VARIABLE_NAME='slave_max_allowed_packet';


--echo '#---------------------FN_DYNVARS_072_07----------------------#'
################################################################### 
#      Check if TRUE and FALSE values can be used on variable     #
################################################################### 

SET @@global.slave_max_allowed_packet = TRUE;
SELECT @@global.slave_max_allowed_packet;
SET @@global.slave_max_allowed_packet = FALSE;
SELECT @@global.slave_max_allowed_packet;


--echo '#---------------------FN_DYNVARS_072_08----------------------#'
########################################################################################################
#    Check if accessing variable with SESSION,LOCAL and without SCOPE points to same session variable  #
########################################################################################################

SET @@global.slave_max_allowed_packet = 5000;
SELECT @@slave_max_allowed_packet = @@global.slave_max_allowed_packet;


--echo '#---------------------FN_DYNVARS_072_09----------------------#'
################################################################################  
#   Check if slave_max_allowed_packet can be accessed with and without @@ sign #
################################################################################

--Error ER_GLOBAL_VARIABLE
SET slave_max_allowed_packet = 6000;
SELECT @@slave_max_allowed_packet;
--Error ER_PARSE_ERROR
SET local.slave_max_allowed_packet = 7000;
--Error ER_UNKNOWN_TABLE
SELECT local.slave_max_allowed_packet;
--Error ER_PARSE_ERROR
SET global.slave_max_allowed_packet = 8000;
--Error ER_UNKNOWN_TABLE
SELECT global.slave_max_allowed_packet;
--Error ER_BAD_FIELD_ERROR
SELECT slave_max_allowed_packet = @@session.slave_max_allowed_packet;


##############################  
#   Restore initial value    #
##############################

SET @@global.slave_max_allowed_packet = @start_value;
SELECT @@global.slave_max_allowed_packet;


######################################################################## 
#              END OF slave_max_allowed_packet TESTS                   #
######################################################################## 

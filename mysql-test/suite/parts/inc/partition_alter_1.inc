################################################################################
# include/partition_alter_1.inc                                                #
#                                                                              #
# Purpose:                                                                     #
#   Alter a partioned table and check the usability afterwards                 #
#   This script is only usefule when sourced by                                #
#        include/partition_alter_1[1|3].inc                                    #
#                                                                              #
#    0. Expect there is a table t1                                             #
#    1. Insert the first half of the table t0_template into t1                 #
#    2. Execute the ALTER TABLE statement within the variable $alter           #
#       Case SQL code in                                                       #
#          0: 1. Insert the second half of the table t0_template into t1       #
#             2. Execute the usability test include/partition_check.inc        #
#          >0, but expected: nothing                                           #
#          >0 and unexpected: abort                                            #
#    3. DROP the table t1                                                      #
#                                                                              #
# The parameter $alter has to be set before sourcing this script.              #
# Example:                                                                     #
#          CREATE TABLE t1 (f_int1 INT,f_int2 INT, .... );                     #
#          let $alter= ALTER TABLE t1 ADD PRIMARY KEY(f_int2);                 #
#          include/partition_alter_1.inc                                       #
#                                                                              #
# The parameters $insert_first_half and $insert_second_half                    #
# are also to be set outside (source ./include/partition.pre).                 #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: ML                                                          #
# Original Date: 2006-03-05                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

eval $insert_first_half;
# Possible/Expected return codes for ALTER TABLE ...
# 0
# 1491: A PRIMARY KEY need to include all fields in the partition function
#       A UNIQUE INDEX need to include all fields in the partition function
--disable_abort_on_error
eval $alter;
--enable_abort_on_error
if ($no_debug)
{
--disable_query_log
}
eval SET @my_errno = $mysql_errno;
let $run_test= `SELECT @my_errno = 0`;
let $unexpected_error= `SELECT @my_errno NOT IN (0,1030,1491,1495)`;
if ($unexpected_error)
{
   --echo #      The last command got an unexepected error response.
   --echo #      Expected/handled SQL codes are 0,1030,1491,1495
   SELECT '#      SQL code we got was: ' AS "", @my_errno AS "";
   --echo #      Sorry, have to abort.
   exit;
   --echo
}
--enable_query_log
# Prevent execution of following usage tests, when ALTER TABLE failed
if ($run_test)
{
eval $insert_second_half;
--source suite/parts/inc/partition_check.inc
# --source include/partition_check.inc
}
DROP TABLE t1;

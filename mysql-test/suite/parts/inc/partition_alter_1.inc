################################################################################
# inc/partition_alter_1.inc                                                    #
#                                                                              #
# Purpose:                                                                     #
#   Alter a partioned table and check the usability afterwards                 #
#   This script is only usefule when sourced by                                #
#        inc/partition_alter_1[1|3].inc                                        #
#                                                                              #
#    0. Expect there is a table t1                                             #
#    1. Insert the first half of the table t0_template into t1                 #
#    2. Execute the ALTER TABLE statement within the variable $alter           #
#       Case SQL code in                                                       #
#          0: 1. Insert the second half of the table t0_template into t1       #
#             2. Execute the usability test inc/partition_check.inc            #
#          >0, but expected: nothing                                           #
#          >0 and unexpected: abort                                            #
#    3. DROP the table t1                                                      #
#                                                                              #
# The parameter $alter has to be set before sourcing this script.              #
# Example:                                                                     #
#          CREATE TABLE t1 (f_int1 INT,f_int2 INT, .... );                     #
#          let $alter= ALTER TABLE t1 ADD PRIMARY KEY(f_int2);                 #
#          inc/partition_alter_1.inc                                           #
#                                                                              #
# The parameters $insert_first_half and $insert_second_half                    #
# are also to be set outside (source ./inc/partition.pre).                     #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date:   2006-03-05                                                  #
# Change Author:   mleich                                                      #
# Change Date:     2007-10-08                                                  #
# Change:          Fix for                                                     #
#                  Bug#31481 test suite parts: Many tests fail because of      #
#                            changed server error codes                        #
################################################################################

eval $insert_first_half;
# Possible/Expected return codes for ALTER TABLE ...
# 0
# 1030: ER_GET_ERRNO
# 1502: ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
# 1506: ER_DROP_PARTITION_NON_EXISTENT
--disable_abort_on_error
eval $alter;
--enable_abort_on_error
if ($no_debug)
{
--disable_query_log
}
eval SET @my_errno = $mysql_errno;
let $run_test= `SELECT @my_errno = 0`;
if (`SELECT @my_errno NOT IN (0,$ER_GET_ERRNO,
                              $ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF,
                              $ER_DROP_PARTITION_NON_EXISTENT)`);
{
   --echo #      The last command got an unexepected error response.
   --echo #      Expected/handled SQL codes are 0,$ER_GET_ERRNO,$ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF,$ER_DROP_PARTITION_NON_EXISTENT
   SELECT '#      SQL code we got was: ' AS "", @my_errno AS "";
   --echo #      Sorry, have to abort.
   --echo #      Please check the error name to number mapping in inc/partition.pre.
   exit;
   --echo
}
--enable_query_log
# Prevent execution of following usage tests, when ALTER TABLE failed
if ($run_test)
{
eval $insert_second_half;
--source suite/parts/inc/partition_check.inc
}
DROP TABLE t1;

################################################################################
# inc/partition_20.inc                                                         #
#                                                                              #
# Purpose:                                                                     #
#   Auxiliary script, only useful when sourced by                              #
#   suite/parts/inc/partition_check.inc.                                       #
#                                                                              #
# 1. Check if the preceding statement caused that the expected number of       #
#    records was                                                               #
#    - inserted                                                                #
#    - updated or deleted+inserted                                             #
# 2. Revert the modifications                                                  #
#                                                                              #
#    The parameters                                                            #
#       @try_count = total number of inserted and updated or deleted+inserted  #
#                    records                                                   #
#       @clash_count = number of records where a DUPLICATE KEY appears         #
#    must be set before sourcing this routine.                                 #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-03-05                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

if ($no_debug)
{
  --disable_query_log
}
eval SELECT '# check unique-$num-a success: ' AS "", COUNT(*) = @clash_count AS ""
FROM t1 WHERE f_charbig = 'was updated';
eval SELECT '# check unique-$num-b success: ' AS "", COUNT(*) = @try_count AS ""
FROM t1 WHERE f_charbig IN ('was updated','was inserted');
--enable_query_log
# Revert the modification
DELETE FROM t1 WHERE f_charbig = 'was inserted';
UPDATE t1 SET f_int1 = CAST(f_char1 AS SIGNED INT),
          f_int2 = CAST(f_char1 AS SIGNED INT),
       f_charbig = CONCAT('===',f_char1,'===')
WHERE f_charbig = 'was updated';
inc $num;

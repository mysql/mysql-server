################################################################################
# inc/partition_trigg2.inc                                                     #
#                                                                              #
# Purpose:                                                                     #
#   Auxiliary script, only useful when sourced by inc/partition_check.inc.     #
#   The trigger uses new values (--> event UPDATE, INSERT only)                #
#                                                                              #
# 1. Create a trigger                                                          #
# 2. Execute a statement, which activates the trigger                          #
# 3. Check the results of the trigger activity                                 #
# 4. Revert the modifications                                                  #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-03-05                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

delimiter |;
eval CREATE TRIGGER trg_2 $event ON t1 FOR EACH ROW
BEGIN
   SET new.f_int1 = $source.f_int1 + @max_row,
       new.f_int2 = $source.f_int2 - @max_row,
       new.f_charbig = '####updated per update trigger####';
END|
delimiter ;|
eval $statement;
#      Check of preceding statement via Select
if ($no_debug)
{
  --disable_query_log
}
eval SELECT '# check trigger-$num success: ' AS "", COUNT(*) = 0 AS "" FROM t1
WHERE f_int1 - CAST(f_char1 AS SIGNED INT) NOT IN (@max_row, 2 * @max_row)
   OR f_int2 - CAST(f_char1 AS SIGNED INT) NOT IN (-@max_row, - 2 * @max_row)
   OR f_charbig <> '####updated per update trigger####';
--enable_query_log
DROP TRIGGER trg_2;
# Revert the changes
eval UPDATE t1 SET f_int1 = CAST(f_char1 AS SIGNED INT),
              f_int2 = CAST(f_char1 AS SIGNED INT),
              f_charbig = CONCAT('===',f_char1,'===');
inc $num;

################################################################################
# inc/partition_trigg3.inc                                                     #
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
# Original version of the trigger
# eval CREATE TRIGGER trg_3 $event ON t1 FOR EACH ROW
# BEGIN
#    SET @counter = 1;
#    SET @my_max1 = 0, @my_max2 = 0;
#    SELECT MAX(f_int1), MIN(f_int2) INTO @my_max1,@my_min2 FROM t1;
#    SET new.f_int1 = @my_max1 + @counter,
#        new.f_int2 = @my_min2 - @counter,
#        new.f_charbig = '####updated per insert trigger####';
#    SET @counter = @counter + 1;
# END|
#
# Bug/currently undocumented limitation:
#      17704: Triggers: MAX, Insert select with several rows, strange error
# "A trigger can not access (not even read data) the table it's defined for."
#
eval CREATE TRIGGER trg_3 $event ON t1 FOR EACH ROW
BEGIN
   SET new.f_int1 = @my_max1 + @counter,
       new.f_int2 = @my_min2 - @counter,
       new.f_charbig = '####updated per insert trigger####';
   SET @counter = @counter + 1;
END|
delimiter ;|
# Additional statements because of Bug(limitation)#17704
SET @counter = 1;
# Bug#18730 Partitions: NDB, crash on SELECT MIN(<unique column>)
SELECT MAX(f_int1), MIN(f_int2) INTO @my_max1,@my_min2 FROM t1;
# Additional statements end
eval $statement;
DROP TRIGGER trg_3;
#      Check of preceding statement via Select
if ($no_debug)
{
  --disable_query_log
}
# We insert records around max_row_div2 !
eval SELECT '# check trigger-$num success: ' AS "", COUNT(*) = 3 AS "" FROM t1
WHERE f_int1 = CAST(f_char1 AS SIGNED INT) + @max_row_div2 + 2
  AND f_int2 = - CAST(f_char1 AS SIGNED INT) + @max_row_div2 - 1
  AND f_charbig = '####updated per insert trigger####';
--enable_query_log
# Revert the changes
eval DELETE FROM t1
WHERE f_int1 <> CAST(f_char1 AS SIGNED INT)
  AND f_int2 <> CAST(f_char1 AS SIGNED INT)
  AND f_charbig = '####updated per insert trigger####';
inc $num;

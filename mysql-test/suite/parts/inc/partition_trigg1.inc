################################################################################
# inc/partition_trigg1.inc                                                     #
#                                                                              #
# Purpose:                                                                     #
#   Auxiliary script, only useful when sourced by inc/partition_check.inc. #
#   One trigger uses new values (--> event UPDATE, INSERT only)                #
#   One trigger uses old values (--> event UPDATE, DELETE only)                #
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

# Test for operations, which have new values (INSERT+UPDATE, but not DELETE)
if ($no_debug)
{
  --disable_query_log
}
eval SELECT INSTR('$statement','DELETE') = 0 INTO @aux;
let $run1= `SELECT @aux`;
--enable_query_log
if ($run1)
{
   # Insert three records which are only needed for UPDATE TRIGGER test
   eval INSERT INTO $tab_has_trigg(f_int1,f_int2,f_char1,f_char2,f_charbig)
   SELECT -f_int1,-f_int1,CAST(-f_int1 AS CHAR),CAST(-f_int1 AS CHAR),
         'just inserted' FROM t0_template
   WHERE f_int1 BETWEEN @max_row_div2 - 1 AND @max_row_div2 + 1;
   delimiter |;
   eval CREATE TRIGGER trg_1 $event ON $tab_has_trigg FOR EACH ROW
   BEGIN
      UPDATE $tab_in_trigg SET f_int1 = -f_int1, f_int2 = -f_int2,
                    f_charbig = 'updated by trigger'
      WHERE f_int1 = new.f_int1;
   END|
   delimiter ;|

   eval $statement;

   #      Check of preceding statement via Select
   if ($no_debug)
   {
     --disable_query_log
   }
   eval SELECT '# check trigger-$num success: ' AS "", COUNT(*) = 3 AS ""
   FROM $tab_in_trigg
   WHERE f_int1 = f_int2 AND CAST(f_char1 AS SIGNED INT) = -f_int1;
   --enable_query_log

   DROP TRIGGER trg_1;

   # Revert the changes
   eval UPDATE $tab_in_trigg SET f_int1 = CAST(f_char1 AS SIGNED INT),
              f_int2 = CAST(f_char1 AS SIGNED INT),
              f_charbig = 'just inserted'
   WHERE f_int1 <> CAST(f_char1 AS SIGNED INT);
   eval DELETE FROM $tab_has_trigg
   WHERE ABS(f_int1) BETWEEN @max_row_div2 - 1 AND @max_row_div2 + 1;
   inc $num;
}

# Test for operations, which have old values (UPDATE+DELETE, but not INSERT)
if ($no_debug)
{
  --disable_query_log
}
eval SELECT INSTR('$statement','INSERT') = 0 INTO @aux;
let $run1= `SELECT @aux`;
--enable_query_log
if ($run1)
{
   # Insert three records which are only needed for UPDATE/DELETE TRIGGER test
   eval INSERT INTO $tab_has_trigg(f_int1,f_int2,f_char1,f_char2,f_charbig)
   SELECT -f_int1,-f_int1,CAST(-f_int1 AS CHAR),CAST(-f_int1 AS CHAR),
          'just inserted' FROM t0_template
   WHERE f_int1 BETWEEN @max_row_div2 - 1 AND @max_row_div2 + 1;

   delimiter |;
   eval CREATE TRIGGER trg_1 $event ON $tab_has_trigg FOR EACH ROW
   BEGIN
      UPDATE $tab_in_trigg SET f_int1 = -f_int1, f_int2 = -f_int2,
                    f_charbig = 'updated by trigger'
      WHERE f_int1 = - old.f_int1;
   END|
   delimiter ;|

   eval $statement;

   #      Check of preceding statement via Select
   if ($no_debug)
   {
     --disable_query_log
   }
   eval SELECT '# check trigger-$num success: ' AS "", COUNT(*) = 3 AS ""
   FROM $tab_in_trigg
   WHERE f_int1 = f_int2 AND CAST(f_char1 AS SIGNED INT) = -f_int1;
   --enable_query_log
   DROP TRIGGER trg_1;
   # Revert the changes
   eval UPDATE $tab_in_trigg SET f_int1 = CAST(f_char1 AS SIGNED INT),
                 f_int2 = CAST(f_char1 AS SIGNED INT),
                 f_charbig = 'just inserted'
   WHERE f_int1 <> CAST(f_char1 AS SIGNED INT);
   eval DELETE FROM $tab_has_trigg
   WHERE ABS(f_int1) BETWEEN @max_row_div2 - 1 AND @max_row_div2 + 1;
   inc $num;
}

# Check that the last statement executed harvested
#
# binlog_format | query_get_value(SHOW WARNINGS LIMIT 1, Message, 1)
# --------------+---------------------------------------------------
#     statement | Unsafe statement written to the binary log.....
#  <> statement | No such row     (no warning at all)
#

let $my_value= query_get_value(SHOW WARNINGS LIMIT 1, Message, 1);
if (`SELECT (NOT ('$my_value' LIKE 'Unsafe statement written to the binary log%'
                  AND @@binlog_format = 'statement'))
            AND
            (NOT ('$my_value' LIKE 'No such row'
                  AND @@binlog_format <> 'statement')) `)
{
   --echo ERROR:
   --echo Last statement is
   --echo - unsafe in case of binlog_format  = 'statement'
   --echo   Expect to get a warning.
   --echo -   safe in case of binlog_format <> 'statement'
   --echo   Expect to get 'No such row'.
   --echo Result of SHOW WARNINGS via query_get_value : '$my_value'
   SELECT @@binlog_format;
   --echo Abort
   exit;
}
let $my_value= 'Garbage';

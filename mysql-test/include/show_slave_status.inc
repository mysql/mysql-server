# Include file to show the slave status, masking out some information
# that varies depending on where the test is executed.

--let $_items=$status_items
if (`SELECT "XX$status_items" = "XX"`)
{
  --die 'Variable status_items is NULL' 
}

--disable_query_log
--vertical_results

while (`SELECT "XX$_items" <> "XX"`) 
{
  --let $_name= `SELECT SUBSTRING_INDEX('$_items', ',', 1)`
  --let $_items= `SELECT LTRIM(SUBSTRING('$_items', LENGTH('$_name') + 2))`

  --let $_value= query_get_value(SHOW SLAVE STATUS, $_name, 1)

  --replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
  --eval SELECT "$_value" AS $_name
}

--horizontal_results
--enable_query_log

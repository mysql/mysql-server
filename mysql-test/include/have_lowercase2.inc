let $lctn= query_get_value(SHOW VARIABLES LIKE 'lower_case_table_names', Value, 1);
if ($lctn != 2)
{
  --skip Test requires 'lower_case_table_names = 2'
}

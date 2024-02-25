let $lcfs = query_get_value(SHOW VARIABLES LIKE 'lower_case_file_system', Value, 1);
if ($lcfs != ON)
{
  --skip Test requires 'case_insensitive_file_system'
}

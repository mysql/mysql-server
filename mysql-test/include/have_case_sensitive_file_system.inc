let $lcfs = query_get_value(SHOW VARIABLES LIKE 'lower_case_file_system', Value, 1);
if ($lcfs != OFF)
{
  --skip Test requires 'case_sensitive_file_system'
}

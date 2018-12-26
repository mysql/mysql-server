let $stmt_timeout= query_get_value(SHOW VARIABLES LIKE 'have_statement_timeout', Value, 1);
if ($stmt_timeout != YES)
{
  --skip Test requires 'have_statement_timeout'
}

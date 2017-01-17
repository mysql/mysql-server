let $local_infile= query_get_value(SHOW VARIABLES LIKE 'local_infile', Value, 1);
if ($local_infile != ON)
{
  --skip Test requires 'have_local_infile'
}

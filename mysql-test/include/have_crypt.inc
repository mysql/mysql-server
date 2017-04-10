let $have_crypt= query_get_value(SHOW VARIABLES LIKE 'have_crypt', Value, 1);
if ($have_crypt != YES)
{
  --skip Test requires 'have_crypt'
}

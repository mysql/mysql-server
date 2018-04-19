let $have_openssl= `SELECT @@have_openssl`;
if ($have_openssl != NO)
{
  --skip Test requires 'not_openssl'
}

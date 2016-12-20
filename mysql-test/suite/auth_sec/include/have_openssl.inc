let $have_openssl= `SELECT @@have_openssl LIKE 'YES%'`;
if (!$have_openssl)
{
  --skip Needs OpenSSL
}

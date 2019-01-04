let $have_ssl= `SELECT @@have_ssl LIKE 'YES'`;
if (!$have_ssl)
{
  --skip Test requires 'have_ssl'
}
--source include/allowed_ciphers.inc

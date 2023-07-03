<<<<<<< HEAD
let $have_ssl= `SELECT @@have_ssl LIKE 'YES'`;
if (!$have_ssl)
{
  --skip Test requires 'have_ssl'
}
=======
-- require r/have_ssl.require
disable_query_log;
show variables like 'have_ssl';
enable_query_log;
--source include/allowed_ciphers.inc
>>>>>>> upstream/cluster-7.6

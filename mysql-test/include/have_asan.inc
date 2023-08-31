let $have_asan = `SELECT VERSION() LIKE '%asan%'`;
if (!$have_asan)
{
  --skip Test requires Address Sanitizer
}

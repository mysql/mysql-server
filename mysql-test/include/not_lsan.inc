let $have_lsan = `SELECT VERSION() LIKE '%lsan%'`;
if ($have_lsan)
{
  --skip Test requires 'not_lsan'
}

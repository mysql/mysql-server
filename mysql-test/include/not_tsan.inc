let $have_tsan = `SELECT VERSION() LIKE '%tsan%'`;
if ($have_tsan)
{
  --skip Test requires 'not_tsan'
}

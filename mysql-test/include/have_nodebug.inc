let $have_debug = `SELECT (VERSION() LIKE '%debug%')`;
if ($have_debug)
{
  --skip Test requires 'have_nodebug'
}

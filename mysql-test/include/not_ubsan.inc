disable_query_log;
let $have_ubsan = `SELECT (VERSION() LIKE '%ubsan%')`;
if ($have_ubsan)
{
  skip Test requires: 'not_ubsan';
}
enable_query_log;

let $have_debug = `SELECT (VERSION() LIKE '%debug%')`;
if ($have_debug)
{
  --skip Test requires binaries to be compiled in release mode
}

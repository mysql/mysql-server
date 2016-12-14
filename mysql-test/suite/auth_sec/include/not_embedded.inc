let $have_embedded = `SELECT VERSION() LIKE '%embedded%'`;
if ($have_embedded)
{
  --skip Not run for embedded server
}

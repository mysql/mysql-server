let $have_embedded = `SELECT (VERSION() LIKE '%embedded%')`;
if (!$have_embedded)
{
  --skip Test requires 'embedded server'
}

let $have_ndb = `SELECT VERSION() LIKE '%cluster%'`;
if ($have_ndb)
{
  --skip Test requires 'not_ndb'
}

let $support= `SELECT (support = 'YES' OR support = 'DEFAULT') FROM information_schema.engines WHERE engine = 'archive'`;
if (!$support)
{
  --skip Test requires 'have_archive'
}

let $is_debug = `select version() like '%debug%'`;
if ($is_debug)
{
  skip Does not run in with debug binaries;
}


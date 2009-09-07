if (!`SELECT count(*) FROM information_schema.engines WHERE
      (support = 'YES' OR support = 'DEFAULT') AND
      engine = 'ibmdb2i'`)
{
  skip Need ibmdb2i engine;
}

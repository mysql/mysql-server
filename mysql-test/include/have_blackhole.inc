if (!`SELECT count(*) FROM information_schema.engines WHERE
      (support = 'YES' OR support = 'DEFAULT') AND
      engine = 'blackhole'`){
  skip Need blackhole engine;
}

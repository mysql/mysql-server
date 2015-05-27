
if (!`SELECT count(*) FROM information_schema.engines WHERE
      engine = 'PERFORMANCE_SCHEMA'`){
  skip Need performance schema compiled in;
}

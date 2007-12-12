if (`select (support = 'YES' or support = 'DEFAULT')=0 from information_schema.engines where engine = 'blackhole'`){
  skip Need blackhole engine;
}

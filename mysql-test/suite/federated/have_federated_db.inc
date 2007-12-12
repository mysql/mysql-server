if (`select (support = 'YES' or support = 'DEFAULT')=0 from information_schema.engines where engine = 'federated'`)
{
  skip Need federated engine;
}

if (`SELECT @@GLOBAL.replica_parallel_workers = 0`)
{
  skip Test makes sense only to run with MTS;
}

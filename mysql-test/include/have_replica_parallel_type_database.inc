if (`SELECT @@GLOBAL.replica_parallel_type != 'DATABASE'`)
{
  skip Test makes sense only to run with MTS replica-parallel-type DATABASE;
}

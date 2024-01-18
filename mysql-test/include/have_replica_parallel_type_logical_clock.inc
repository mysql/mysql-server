if (`SELECT @@GLOBAL.replica_parallel_type != 'LOGICAL_CLOCK'`)
{
  skip Test makes sense only to run with MTS replica-parallel-type LOGICAL_CLOCK;
}

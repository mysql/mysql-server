if (`SELECT @@GLOBAL.slave_parallel_type != 'LOGICAL_CLOCK'`)
{
  skip Test makes sense only to run with MTS slave-parallel-type LOGICAL_CLOCK;
}

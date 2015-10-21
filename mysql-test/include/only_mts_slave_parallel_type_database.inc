if (`SELECT @@GLOBAL.slave_parallel_type != 'DATABASE'`)
{
  skip Test makes sense only to run with MTS slave-parallel-type DATABASE;
}

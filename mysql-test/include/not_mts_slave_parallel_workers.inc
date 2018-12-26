if (`SELECT @@GLOBAL.slave_parallel_workers != 0`)
{
     skip Test cannot run with MTS;
}

if (`SELECT @@GLOBAL.master_info_repository = 'TABLE'`)
{
     skip Test cannot run with master.info repository table;
}

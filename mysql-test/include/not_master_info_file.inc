if (`SELECT @@GLOBAL.master_info_repository = 'FILE'`)
{
     skip Test cannot run with master.info repository file;
}

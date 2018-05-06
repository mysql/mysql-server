if (`SELECT @@GLOBAL.relay_log_info_repository = 'FILE'`)
{
     skip Test cannot run with relay_log.info repository file;
}

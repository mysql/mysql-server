if (`SELECT @@GLOBAL.relay_log_info_repository = 'TABLE'`)
{
     skip Test cannot run with relay_log.info repository table;
}

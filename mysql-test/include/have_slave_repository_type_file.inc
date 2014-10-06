--let $mi_repo= `SELECT @@GLOBAL.master_info_repository='TABLE'`
--let $rli_repo= `SELECT @@GLOBAL.relay_log_info_repository='TABLE'`

if (`SELECT $mi_repo = 1 OR $rli_repo = 1`)
{
  --skip Test runs only when slave repository type = FILE
}

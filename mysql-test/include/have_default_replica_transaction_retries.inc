--let $retry_num = `SELECT @@GLOBAL.replica_transaction_retries`
if ($retry_num != 'DEFAULT')
{
   --skip Test requires requires replica_transaction_retries = default
}

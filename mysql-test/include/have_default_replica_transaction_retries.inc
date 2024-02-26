--let $retry_num = `SELECT @@GLOBAL.replica_transaction_retries`
if ($retry_num != 10)
{
   --skip Test requires replica_transaction_retries = default
}

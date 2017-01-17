let $one_thread_per_conn= `SELECT @@thread_handling LIKE 'one-thread-per-connection'`;
if (!$one_thread_per_conn)
{
  --skip Test requires 'one_thread_per_connection'
}

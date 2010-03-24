disable_query_log;
disable_result_log;
set @have_thread_concurrency=0;
select @have_thread_concurrency:=1 from information_schema.global_variables where variable_name='thread_concurrency';
if (`select @have_thread_concurrency = 0`)
{
  skip Need @@thread_concurrency;
}
enable_result_log;
enable_query_log;

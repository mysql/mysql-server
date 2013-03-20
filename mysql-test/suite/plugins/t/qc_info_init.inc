if (`select count(*) = 0 from information_schema.plugins where plugin_name = 'query_cache_info' and plugin_status='active'`)
{
  --skip QUERY_CACHE_INFO plugin is not active
}

set global query_cache_size=1355776;

create table t1 (a int not null);
insert into t1 values (1),(2),(3);
select * from t1;
select statement_schema, statement_text, result_blocks_count, result_blocks_size from information_schema.query_cache_info;


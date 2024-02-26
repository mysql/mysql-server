--disable_query_log
--disable_result_log
#save the old memory stats in case if it exists
select @var_used_bytes into @old_var_used_bytes;
#now fetch the new memory stats
--source suite/ndb/include/ndb_info.inc
eval
select sum(var_elem_alloc_bytes - var_elem_free_bytes)/$MTR_NDB_NO_OF_REPLICAS
       into @var_used_bytes
       from ndbinfo.memory_per_fragment
       where fq_name like '%t1'
       group by table_id;
--enable_result_log
--enable_query_log

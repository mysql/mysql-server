--disable_query_log
--disable_result_log

# Initialise @start_lock_reqs count
# See ndbinfo_wait_frag_lock_count.inc

select 
  @start_lock_reqs:=sum(ex_req) + sum(sh_req)
  from locks_per_fragment
  where 
    fq_name = 'test/def/t1' or
    parent_fq_name = 'test/def/t1';


--enable_result_log
--enable_query_log

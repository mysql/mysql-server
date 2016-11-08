--disable_query_log
--disable_result_log


# Wait for a lock request to arrive, relative
# to @start_lock_reqs

let $ready=0;

while(!$ready)
{
  select 
    @total_lock_reqs:=sum(ex_req) + sum(sh_req)
    from locks_per_fragment
    where 
      fq_name = 'test/def/t1' or
      parent_fq_name = 'test/def/t1';

  select
    @ready:=(@total_lock_reqs > @start_lock_reqs);
  
  let $ready=query_get_value(select @ready as r, r, 1);

  sleep 0.1;
}



--enable_result_log
--enable_query_log

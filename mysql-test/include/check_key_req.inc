# include file for checking if variable key_reads = key_read_requests
let $key_reads= query_get_value(SHOW STATUS LIKE 'key_reads',Value,1);
let $key_r_req= query_get_value(SHOW STATUS LIKE 'key_read_requests',Value,1);
let $key_writes= query_get_value(SHOW STATUS LIKE 'key_writes',Value,1);
let $key_w_req= query_get_value(SHOW STATUS LIKE 'key_write_requests',Value,1);
--disable_query_log
eval SELECT IF($key_reads = $key_r_req, "reads == requests", "reads != requests") as 'reads vs requests';
eval SELECT IF($key_writes = $key_w_req, "writes == requests", "writes != requests") as 'writes vs requests';
--enable_query_log

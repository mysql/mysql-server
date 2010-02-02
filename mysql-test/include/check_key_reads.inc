# include file for checking if variable key_reads is zero
let $key_reads= query_get_value(SHOW STATUS LIKE 'key_reads',Value,1);
--disable_query_log
eval SELECT IF($key_reads = 0, "Yes!", "No!") as 'Zero key reads?';
FLUSH STATUS;
--enable_query_log

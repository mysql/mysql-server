# Created by Horst Hunger       2008-04-15
# see also have_32bit_ulong.inc

--disable_query_log
let $save = `SELECT @@pseudo_thread_id`;
SET @@pseudo_thread_id = 4294967296;
let $mach64 = `SELECT @@pseudo_thread_id > 4294967295`;
eval SET @@pseudo_thread_id = $save;
--enable_query_log
if (!$mach64)
{
  skip Need a 64 unsigned long ;
}


# Created by Horst Hunger     2008-04-15
# see also have_64bit_ulong.inc

--disable_query_log
--disable_warnings
let $save = `SELECT @@pseudo_thread_id`;
SET @@pseudo_thread_id = 4294967296;
let $mach32 = `SELECT @@pseudo_thread_id <= 4294967295`;
eval SET @@pseudo_thread_id = $save;
--enable_warnings
--enable_query_log
if (!$mach32)
{
  skip Need a 32 bit unsigned long;
}


# Created by Horst Hunger     2008-04-15
# see also have_64bit.inc

--disable_query_log
--disable_warnings
let $save = `SELECT @@global.sort_buffer_size`;
SET @@global.sort_buffer_size = 4294967296;
let $mach32 = `SELECT @@global.sort_buffer_size <= 4294967295`;
eval SET @@global.sort_buffer_size = $save;
--enable_warnings
--enable_query_log
if (!$mach32)
{
  skip Need a 32 bit machine/binary;
}


# Created by Horst Hunger       2008-04-15
# see also have_32bit.inc

--disable_query_log
let $save = `SELECT @@session.sort_buffer_size`;
SET @@session.sort_buffer_size = 4294967296;
let $mach64 = `SELECT @@session.sort_buffer_size > 4294967295`;
eval SET @@session.sort_buffer_size = $save;
--enable_query_log
if (!$mach64)
{
  skip Need a 64 binary ;
}


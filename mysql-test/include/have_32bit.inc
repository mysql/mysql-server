# Created by Horst Hunger       2008-04-15
# Modified by Pavan Naik        2015-12-11
# see also have_32bit.inc

--disable_query_log
--disable_warnings
let $save = `SELECT @@session.myisam_sort_buffer_size`;
SET @@session.myisam_sort_buffer_size = 4294967296;
let $mach32 = `SELECT @@session.myisam_sort_buffer_size <= 4294967295`;
eval SET @@session.myisam_sort_buffer_size = $save;
--enable_warnings
--enable_query_log

if (!$mach32)
{
  skip Need a 32 binary;
}

# Created by Horst Hunger       2008-04-15
# Modified by Pavan Naik        2015-12-11
# see also have_32bit.inc

--disable_query_log
let $save = `SELECT @@session.myisam_sort_buffer_size`;
SET @@session.myisam_sort_buffer_size = 4294967296;
let $mach64 = `SELECT @@session.myisam_sort_buffer_size > 4294967295`;
eval SET @@session.myisam_sort_buffer_size = $save;
--enable_query_log

if (!$mach64)
{
  skip Need a 64 binary;
}

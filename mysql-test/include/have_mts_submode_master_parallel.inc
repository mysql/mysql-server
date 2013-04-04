#
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
#
--disable_query_log
--disable_result_log
--let $submode= query_get_value(SHOW VARIABLES LIKE "%slave_parallel_type%", Value, 1)
if ($submode != 'MASTER_PARALLEL')
{
  --skip The test needs slave_parallel_type=master_parallel
}
--enable_result_log
--enable_query_log


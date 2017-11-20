--source include/resource_group_init.inc

if ($thr_prio_enable == 1)
{
  skip Skipping test. (Test requires thread priority to be not supported.);
}


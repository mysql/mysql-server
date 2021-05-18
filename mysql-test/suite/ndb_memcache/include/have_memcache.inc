
--source include/not_windows.inc

if($NDB_MEMCACHED_STARTED != 1)
{
  skip Memcached is not running;
}

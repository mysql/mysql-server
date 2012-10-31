let $memcache_check = `select (PLUGIN_LIBRARY LIKE 'libmemcached%') FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME='daemon_memcached'`;
if ( $memcache_check != 1 )
{
   --skip Test requires: Perl Module Memcached.pm
}


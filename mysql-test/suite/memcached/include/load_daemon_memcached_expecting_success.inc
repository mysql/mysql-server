# Loads the daemon_memcached plugin and waits for it to
# become online and responsive to 'stats' command.
#
# Example of usage:
#   # host and port on which the memcached should operate
#   # ususally host is 127.0.0.1 and
#   # port is in -master.opt file as --loose-daemon_memcached_option
#   --let $memcached_address=127.0.0.1:11227
#   --source ../include/load_daemon_memcached_expecting_success.inc

INSTALL PLUGIN daemon_memcached SONAME 'libmemcached.so';

--let $memcached_check_attempts=20
--let $memcached_expect=success
--source ../include/check_daemon_memcached.inc


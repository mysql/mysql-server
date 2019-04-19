# Loads the daemon_memcached plugin and waits for a while
# to make sure it will not become responsive to 'stats'
#
# Example of usage:
#   # host and port on which the memcached should operate
#   # ususally host is 127.0.0.1 and
#   # port is in -master.opt file as --loose-daemon_memcached_option
#   --let $memcached_address=127.0.0.1:11227
#   --source ../include/load_daemon_memcached_expecting_error.inc

INSTALL PLUGIN daemon_memcached SONAME 'libmemcached.so';
# We use a rather small number here, not to slow down tests, yet
# 3 seconds should be enough at least on some runs to expose a
# problem when the server starts even though it should not.
# In my experience 90% of the time the plugin starts in less than
# 2 seconds, as evidenced by old vesions of code using SLEEP(2).
--let $memcached_check_attempts=3
--let $memcached_expect=error
--source ../include/check_daemon_memcached.inc

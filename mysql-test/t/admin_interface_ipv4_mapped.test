# The box should support IPv4-mapped addresses
--source include/have_ipv4_mapped.inc

--disable_query_log
CALL mtr.add_suppression("Failed to initialize TLS for channel: mysql_admin");
CALL mtr.add_suppression("Failed to set up SSL because of the following SSL library error");
--enable_query_log

# Check that if the admin interface is bound to an IPv4-mapped IPv6-address,
# the server accepts client connections from both IPv4 and IPv6 worlds.

--let $restart_parameters=restart: --skip-name-resolve --admin-address=::ffff:127.0.0.1 --admin_port=$MASTER_ADMINPORT
--replace_result $MASTER_ADMINPORT ADMIN_PORT
--source include/restart_mysqld.inc

--source include/add_extra_root_users.inc

--echo # Checking ::ffff:127.0.0.1 ...
--exec $MYSQLADMIN --no-defaults -h ::ffff:127.0.0.1 -P $MASTER_ADMINPORT -u root ping

--echo # Checking 127.0.0.1 ...
--exec $MYSQLADMIN --no-defaults -h 127.0.0.1 -P $MASTER_ADMINPORT -u root ping

--source include/remove_extra_root_users.inc

# NOTE: ::1 has nothing to do with ::ffff:127.0.0.1, so the server bound to
# ::ffff:127.0.0.1 will not accept connections from to ::1.

--echo #
--echo # Starting mysqld in the regular mode...
--echo #
--let $restart_parameters=
--connection default
--source include/restart_mysqld.inc

--source include/have_debug.inc
--source include/have_ipv6.inc

# Thread stack overrun on solaris
--source include/not_sparc_debug.inc

--echo
--echo # Suppress warnings if IP address cannot not be resolved.
call mtr.add_suppression("2001:db8::6:6");

--echo
--echo # Enforce a clean state
--source suite/perfschema/include/wait_for_pfs_thread_count.inc
--source suite/perfschema/include/hostcache_set_state.inc
TRUNCATE TABLE performance_schema.accounts;

--echo
--echo #########################################################################
--echo #
--echo # Bug#29704941 CONNECTION PASSES FOR HOSTNAME LONGER THAN 255 CHARACTERS
--echo #
--echo #########################################################################

--echo
--echo # Simulate fake ipv6 host address 2001:db8::6:6 and create a user.
SET GLOBAL DEBUG = '+d, vio_peer_addr_fake_ipv6, getaddrinfo_fake_good_ipv6';
CREATE USER 'morty'@'2001:db8::6:6';

--echo
--echo # CASE 1: Simulate hostname length = HOSTNAME_LENGTH + 1 characters.
--echo # The hostname will be "aaaa..... <upto 256 chars>".
SET GLOBAL DEBUG = '+d, getnameinfo_fake_max_length_plus_1';

--echo # Try to connect. Should not be allowed.
--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_HOSTNAME_TOO_LONG
--connect(con1, "::1", morty,, test, $MASTER_MYPORT)
SET GLOBAL DEBUG = '-d, getnameinfo_fake_max_length_plus_1';

--echo
--echo # CASE 2: Simulate hostname length = HOSTNAME_LENGTH characters.
--echo # The hostname will be "aaaa..... <upto 255 chars>".
SET GLOBAL DEBUG = '+d, getnameinfo_fake_max_length';

--echo # Try to connect. Should be allowed.
--connect(con1, "::1", morty,, test, $MASTER_MYPORT)
SELECT CURRENT_USER();

--echo
--echo # Check for successfully connected host in P_S.
--connection default
SHOW VARIABLES LIKE 'performance_schema';
SELECT host FROM performance_schema.hosts WHERE host LIKE 'aaaa%';
SELECT user, host FROM performance_schema.accounts WHERE user='morty';
SELECT ip, host FROM performance_schema.host_cache WHERE host LIKE 'aaaa%';
SET GLOBAL DEBUG = '-d, getnameinfo_fake_max_length';
SET GLOBAL DEBUG = '-d, vio_peer_addr_fake_ipv6, getaddrinfo_fake_good_ipv6';

--echo
--echo # Clean up
--disconnect con1
DROP USER 'morty'@'2001:db8::6:6';
SET GLOBAL DEBUG = default;

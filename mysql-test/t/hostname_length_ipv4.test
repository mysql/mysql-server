--source include/have_debug.inc

# Thread stack overrun on solaris
--source include/not_sparc_debug.inc

--echo
--echo # Suppress warnings if IP address cannot not be resolved.
call mtr.add_suppression("192.0.2.4");

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
--echo # Simulate fake ipv4 host address 192.0.2.4 and create a user.
SET GLOBAL DEBUG = '+d, vio_peer_addr_fake_ipv4, getaddrinfo_fake_good_ipv4';
CREATE USER 'rick'@'192.0.2.4';

--echo
--echo # CASE 1: Simulate hostname length = HOSTNAME_LENGTH + 1 characters.
--echo # The hostname will be "aaaa..... <upto 256 chars>".
SET GLOBAL DEBUG = '+d, getnameinfo_fake_max_length_plus_1';

--echo # Try to connect. Should not be allowed.
--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_HOSTNAME_TOO_LONG
--connect(con1, "127.0.0.1", rick,, test, $MASTER_MYPORT)
SET GLOBAL DEBUG = '-d, getnameinfo_fake_max_length_plus_1';

--echo
--echo # CASE 2: Simulate hostname length = HOSTNAME_LENGTH characters.
--echo # The hostname will be "aaaa..... <upto 255 chars>".
SET GLOBAL DEBUG = '+d, getnameinfo_fake_max_length';

--echo # Try to connect. Should be allowed.
--connect(con1, "127.0.0.1", rick,, test, $MASTER_MYPORT)
SELECT CURRENT_USER();

--echo
--echo # Check for successfully connected host in P_S.
--connection default
SHOW VARIABLES LIKE 'performance_schema';
SELECT host FROM performance_schema.hosts WHERE host LIKE 'aaaa%';
SELECT user, host FROM performance_schema.accounts WHERE user='rick';
SELECT ip, host FROM performance_schema.host_cache WHERE host LIKE 'aaaa%';
SET GLOBAL DEBUG = '-d, getnameinfo_fake_max_length';
SET GLOBAL DEBUG = '-d, vio_peer_addr_fake_ipv4, getaddrinfo_fake_good_ipv4';

--echo
--echo # Clean up
--disconnect con1
DROP USER 'rick'@'192.0.2.4';
SET GLOBAL DEBUG = default;

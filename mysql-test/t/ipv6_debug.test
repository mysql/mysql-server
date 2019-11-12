--source include/have_debug.inc

--echo ###
--echo ### Bug#29374606 MEMORY LEAK IN TCP_SOCKET::GET_LISTENER_SOCKET()
--echo ###

--echo # The master.opt file ensures that the server is started with
--echo # --debug option and --bind-address=* to trigger
--echo # leak situation. This simulates that ipv6 socket creation fails,
--echo # even if address resolution succeeds. Previously this would result
--echo # in the ipv6 addrinfo being leaked and this would manifest as
--echo # valgrind/asan errors.

SELECT @@global.debug, @@global.bind_address;

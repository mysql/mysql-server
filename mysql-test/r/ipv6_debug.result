###
### Bug#29374606 MEMORY LEAK IN TCP_SOCKET::GET_LISTENER_SOCKET()
###
# The master.opt file ensures that the server is started with
# --debug option and --bind-address=* to trigger
# leak situation. This simulates that ipv6 socket creation fails,
# even if address resolution succeeds. Previously this would result
# in the ipv6 addrinfo being leaked and this would manifest as
# valgrind/asan errors.
SELECT @@global.debug, @@global.bind_address;
@@global.debug	@@global.bind_address
d,sim_ipv6_unavailable	*

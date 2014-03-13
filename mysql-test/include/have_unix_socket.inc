--source include/not_embedded.inc

if (!$AUTH_SOCKET_SO) {
  skip No unix_socket plugin;
}

if (!$USER) {
  skip USER variable is undefined;
}


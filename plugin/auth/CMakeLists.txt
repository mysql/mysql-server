MYSQL_ADD_PLUGIN(auth dialog.c
  MODULE_ONLY)

CHECK_CXX_SOURCE_COMPILES(
"#define _GNU_SOURCE
#include <sys/socket.h>
int main() {
  struct ucred cred;
  getsockopt(0, SOL_SOCKET, SO_PEERCRED, &cred, 0);
}" HAVE_PEERCRED)

IF(HAVE_PEERCRED)
  MYSQL_ADD_PLUGIN(auth_socket auth_socket.c
    MODULE_ONLY)
ENDIF(HAVE_PEERCRED)

#include "listener.h"
#include "thread_repository.h"
#include "log.h"

C_MODE_START

pthread_handler_decl(listener, arg)
{
  Thread_info info(pthread_self());
  Thread_repository &thread_repository=
    ((Listener_thread_args *) arg)->thread_repository;
  thread_repository.register_thread(&info);

  while (true)
  {
    log_info("listener is alive");
    sleep(2);
    if (thread_repository.is_shutdown())
      break;
  }
  log_info("listener(): shutdown requested, exiting...");

  thread_repository.unregister_thread(&info);
  return 0;
}

C_MODE_END

#if 0
  while (true)
  {
  }
  /*
    Dummy manager implementation: listens on a UNIX socket and
    starts echo server in a dedicated thread for each accepted connection.
    Enough to test startup/shutdown/options/logging of the instance manager.
  */

  int fd= socket(AF_UNIX, SOCK_STREAM, 0);
  
  if (!fd)
    die("socket(): failed");

  struct sockaddr_un address;
  bzero(&address, sizeof(address));
  address.sun_family= AF_UNIX;
  strcpy(address.sun_path, socket_path);
  int opt= 1;

  if (unlink(socket_path) ||
      bind(fd, (struct sockaddr *) &address, sizeof(address)) ||
      setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    die("unlink | bind | setsockopt failed");
  
  if (listen(fd, 5))
    die("listen() failed");
  
  int client_fd;
  while ((client_fd= accept(fd, 0, 0)) != -1);
  {
    printf("accepted\n");
    const char *message= "\n10hel";
    send(client_fd, message, strlen(message), 0);

    int sleep_seconds= argc > 1 && atoi(argv[1]) ? atoi(argv[1]) : 1;
    printf("sleeping %d seconds\n", sleep_seconds);
    sleep(sleep_seconds);
    close(client_fd);
  }
  printf("accept(): failed\n");
  close(fd);
#endif

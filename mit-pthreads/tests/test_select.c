#include <pthread.h>
#include <stdio.h>
#ifndef ultrix
#include <sys/fcntl.h>
#else /* ultrix */
#include <fcntl.h>
#endif /* !ultrix */
#include <sys/types.h>
#include <sys/time.h>
#ifdef hpux
#include <sys/file.h>
#endif /* hpux */
#include <errno.h>
#define NLOOPS 1000

int ntouts = 0;

void *
bg_routine(void *arg)
{
  write(1,"bg routine running\n",19);
  /*pthread_dump_state();*/
  while (1) {
    int n;
    char dot;

    dot = '.';
    pthread_yield();
    write(1,&dot,1);
    pthread_yield();
    n = NLOOPS;
    while (n-- > 0)
      pthread_yield();
  }
}

void *
fg_routine(void *arg)
{
  int flags, stat, nonblock_flag;
  static struct timeval tout = { 0, 500000 };

#if 0
#if defined(hpux) || defined(__alpha)
  nonblock_flag = O_NONBLOCK;
#else
  nonblock_flag = FNDELAY;
#endif
  printf("fg_routine running\n");
  flags = fcntl(0, F_GETFL, 0);
  printf("stdin flags b4 anything = %x\n", flags);
  stat = fcntl(0, F_SETFL, flags | nonblock_flag);
  if (stat < 0) {
    printf("fcntl(%x) => %d\n", nonblock_flag, errno);
    printf("could not set nonblocking i/o on stdin [oldf %x, stat %d]\n",
	   flags, stat);
    exit(1);
  }
  printf("stdin flags = 0x%x after turning on %x\n", flags, nonblock_flag);
#endif
  while (1) {
    int n;
    fd_set r;

    FD_ZERO(&r);
    FD_SET(0,&r);
    printf("select>");
    n = select(1, &r, (fd_set*)0, (fd_set*)0, (struct timeval *)0);
    if (n < 0) {
      perror ("select");
      exit(1);
    } else if (n > 0) {
      int nb;
      char buf[128];

      printf("=> select returned: %d\n", n);
      while ((nb = read(0, buf, sizeof(buf)-1)) >= 0) {
	buf[nb] = '\0';
	printf("read %d: |%s|\n", nb, buf);
      }
      printf("=> out of read loop: %d / %d\n", nb, errno);
      if (nb < 0) {
	if (errno != EWOULDBLOCK && errno != EAGAIN) {
	  perror ("read");
	  exit(1);
	}
      }
    } else
      ntouts++;
  }
}

main(int argc, char **argv)
{
  pthread_t bg_thread, fg_thread;
  int junk;

  pthread_init();
  setbuf(stdout,NULL);
  setbuf(stderr,NULL);
  if (argc > 1) {
    if (pthread_create(&bg_thread, NULL, bg_routine, 0) < 0) {
      printf("error: could not create bg thread\n");
      exit(1);
    }
  }
  if (pthread_create(&fg_thread, NULL, fg_routine, 0) < 0) {
    printf("error: could not create fg thread\n");
    exit(1);
  }
  printf("threads forked: bg=%lx fg=%lx\n", bg_thread, fg_thread);
  /*pthread_dump_state();*/
  printf("initial thread %lx joining fg...\n", pthread_self());
  pthread_join(fg_thread, (void **)&junk);
}

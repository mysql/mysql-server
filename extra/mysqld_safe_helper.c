#include <my_global.h>
#include <m_string.h>
#include <my_sys.h>
#include <my_pthread.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <stdlib.h>
#include <stdio.h>

void my_exit(int c)
{
  my_end(0);
  exit(c);
}

void do_usage()
{
  printf("Usage:\n"
         "  %s <user> log  <filename>\n"
         "  %s <user> exec <command> <args>\n",
         my_progname, my_progname);
  my_exit(1);
}

void do_log(const char *logfile)
{
  FILE *f;
  uchar buf[4096];
  int size;

  if (!logfile)
    do_usage();

  f= my_fopen(logfile, O_WRONLY|O_APPEND|O_CREAT, MYF(MY_WME));
  if (!f)
    my_exit(1);

  while ((size= my_fread(stdin, buf, sizeof(buf), MYF(MY_WME))) > 0)
    if ((int)my_fwrite(f, buf, size, MYF(MY_WME)) != size)
      my_exit(1);

  my_fclose(f, MYF(0));
  my_exit(0);
}

void do_exec(char *args[])
{
  if (!args[0])
    do_usage();

  my_end(0);
  execvp(args[0], args);
}

int main(int argc, char *argv[])
{
  struct passwd *user_info;
  MY_INIT(argv[0]);

  if (argc < 3)
    do_usage(argv[0]);

  user_info= my_check_user(argv[1], MYF(0));
  if (user_info ? my_set_user(argv[1], user_info, MYF(MY_WME))
                : my_errno == EINVAL)
    my_exit(1);

  if (strcmp(argv[2], "log") == 0)
    do_log(argv[3]);

  if (strcmp(argv[2], "exec") == 0)
    do_exec(argv+3);

  my_end(0);
  return 1;
}

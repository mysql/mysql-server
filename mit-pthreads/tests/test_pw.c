#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>

main()
{
  struct passwd *pw;

  pthread_init();
  pw = getpwuid(getuid());
  if (!pw) {
    printf("getpwuid(%d) died!\n", getuid());
    exit(1);
  }
  printf("getpwuid(%d) => %lx\n", getuid(), pw);
  printf("  you are: %s\n  uid: %d\n  gid: %d\n  class: %s\n  gecos: %s\n  dir: %s\n  shell: %s\n",
	 pw->pw_name, pw->pw_uid, pw->pw_gid, pw->pw_class, pw->pw_gecos, pw->pw_dir,
	 pw->pw_shell);
  exit(0);
}

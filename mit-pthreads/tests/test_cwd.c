#include <stdio.h>

main(int argc, char **argv)
{
    char wd[1024], *getcwd(), *getwd();

    pthread_init();
    printf("getcwd => %s\n", getcwd(wd, 1024));
    printf("getwd => %s\n", getwd(wd));
    exit(0);
}

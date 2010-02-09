#include <stdio.h>
#include <stdlib.h>
#include <toku_assert.h>
#include <fcntl.h>
#include <windows.h>
#include <winsock.h>

int usleep(SOCKET s, unsigned int useconds) {
    fd_set dummy;
    struct timeval tv;
    FD_ZERO(&dummy);
    FD_SET(s, &dummy);
    tv.tv_sec = useconds / 1000000;
    tv.tv_usec = useconds % 1000000;
    return select(0, 0, 0, &dummy, &tv);
}

#include <test.h>
int verbose;

int test_main(int argc, char *argv[]) {
    int i;
    int n = 1;
    WSADATA wsadata;
    SOCKET s;

    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0)
            verbose++;
        n = atoi(arg);
    }

    WSAStartup(MAKEWORD(1, 0), &wsadata);
    s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    printf("s=%"PRIu64"\n", s);

    for (i=0; i<1000; i++) {
        if (verbose) {
            printf("usleep %d\n", i); fflush(stdout);
        }
        usleep(s, n);
    }

    return 0;
}

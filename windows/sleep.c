#include <toku_portability.h>
#include <windows.h>
#include <unistd.h>

unsigned int
sleep(unsigned int seconds) {
    unsigned int m = seconds / 1000000;
    unsigned int n = seconds % 1000000;
    unsigned int i;
    for (i=0; i<m; i++)
        Sleep(1000000*1000);
    Sleep(n*1000);
    return 0;
}

int
usleep(unsigned int useconds) {
    unsigned int m = useconds / 1000;
    unsigned int n = useconds % 1000;
    if (m == 0 && n > 0)
        m = 1;
    Sleep(m);
    return 0;
}


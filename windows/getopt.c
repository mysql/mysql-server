#include <unistd.h>

char *optarg;

static const char *match(char c, const char *optstring) {
    int i;
    for (i=0;optstring[i]; i++)
        if (c == optstring[i])
            return &optstring[i];
    return 0;
}

int getopt(int argc, char * const argv[], const char *optstring) {
    static int lastargc = 0;
    char *arg;
    const char *theopt;
    if (lastargc == 0) {
        lastargc = 1;
    }
    optarg = 0;
    if (lastargc >= argc) {
        lastargc = 0;
        return -1;
    }
    arg = argv[lastargc++];
    if (arg[0] != '-') {
        lastargc = 0;
        return -1;
    }
    theopt = match(arg[1], optstring);
    if (!theopt) {
        lastargc = 0;
        return -1;
    }
    if (theopt[1] == ':') {
        if (arg[2])
            optarg = &arg[2];
        else if (lastargc >= argc) {
            lastargc = 0;
            return -1;
        } else
            optarg = argv[lastargc++];
    }
    return theopt[0];
}


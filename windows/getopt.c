#include <unistd.h>

char *optarg;
int optind;

static const char *match(char c, const char *optstring) {
    int i;
    for (i=0;optstring[i]; i++)
        if (c == optstring[i])
            return &optstring[i];
    return 0;
}

int getopt(int argc, char * const argv[], const char *optstring) {
    static int nextargc = 0;
    static int nextchar = 0;
    char *arg;
    const char *theopt;
    if (nextargc == 0) {
        nextargc = 1;
        optind = 1;
        nextchar = 0;
    }
again:
    optarg = 0;
    if (nextargc >= argc) {
        nextargc = 0;
        return -1;
    }
    arg = argv[nextargc];
    if (!nextchar) {
        arg = argv[nextargc];
        if (arg[0] != '-') {
            nextargc = 0;
            return -1;
        }
        nextchar = 1;
    }
    theopt = match(arg[nextchar++], optstring);
    if (!theopt) {
        nextargc++;
        nextchar = 0;
        goto again;
    }
    if (theopt[1] == ':') {
        if (arg[nextchar]) {
            optarg = &arg[nextchar];
            nextargc++;
            nextchar = 0;
        } else if (nextargc >= argc) {
            nextargc++;
            nextchar = 0;
            return -1;
        } else {
            nextargc++;
            nextchar = 0;
            optarg = argv[nextargc++];
        }
    }
    optind = nextargc;
    return theopt[0];
}

    

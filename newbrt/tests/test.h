#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "toku_portability.h"
#include "brt.h"

#define CKERR(r) do { if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, strerror(r)); assert(r==0); } while (0)
#define CKERR2(r,r2) do { if (r!=r2) fprintf(stderr, "%s:%d error %d %s, expected %d\n", __FILE__, __LINE__, r, strerror(r), r2); assert(r==r2); } while (0)
#define CKERR2s(r,r2,r3) do { if (r!=r2 && r!=r3) fprintf(stderr, "%s:%d error %d %s, expected %d or %d\n", __FILE__, __LINE__, r, strerror(r), r2,r3); assert(r==r2||r==r3); } while (0)

int verbose=0;

void
unlink_file_and_bit(const char *name) {
    char dirty[strlen(name) + sizeof(".dirty")];
    char clean[strlen(name) + sizeof(".clean")];
    sprintf(dirty, "%s.dirty", name);
    sprintf(clean, "%s.clean", name);
    unlink(name);
    unlink(dirty);
    unlink(clean);
}

static inline void
default_parse_args (int argc, const char *argv[]) {
    const char *progname=argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0],"-v")==0) {
	    verbose=1;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose=0;
	} else {
	    fprintf(stderr, "Usage:\n %s [-v] [-q]\n", progname);
	    exit(1);
	}
	argc--; argv++;
    }
}

int test_main(int argc, const char *argv[]);

int
main(int argc, const char *argv[]) {
    toku_brt_init();
    int r = test_main(argc, argv);
    toku_brt_destroy();
    return r;
}


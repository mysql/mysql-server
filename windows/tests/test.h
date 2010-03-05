#include <toku_portability.h>
#include <toku_assert.h>

#define CKERR(r) do { if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, strerror(r)); assert(r==0); } while (0)
#define CKERR2(r,r2) do { if (r!=r2) fprintf(stderr, "%s:%d error %d %s, expected %d\n", __FILE__, __LINE__, r, strerror(r), r2); assert(r==r2); } while (0)
#define CKERR2s(r,r2,r3) do { if (r!=r2 && r!=r3) fprintf(stderr, "%s:%d error %d %s, expected %d or %d\n", __FILE__, __LINE__, r, strerror(r), r2,r3); assert(r==r2||r==r3); } while (0)

#define DEBUG_LINE do { \
    fprintf(stderr, "%s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__); \
    fflush(stderr); \
} while (0)

int test_main(int argc, char *const argv[]);

int
main(int argc, char *const argv[]) {
    int ri = toku_portability_init();
    assert(ri==0);
    int r = test_main(argc, argv);
    int rd = toku_portability_destroy();
    assert(rd==0);
    return r;
}


/* We are going to test whether create and close properly check their input. */

#include "test.h"

enum { MAX_LOCKS = 1000, MAX_LOCK_MEMORY = MAX_LOCKS * 64 };

static void do_ltm_status(toku_ltm *ltm) {
    LTM_STATUS_S s;
    toku_ltm_get_status(ltm, &s);
    assert(s.status[LTM_LOCKS_LIMIT].value.num == MAX_LOCKS);
    assert(s.status[LTM_LOCKS_CURR].value.num == 0);
    assert(s.status[LTM_LOCK_MEMORY_LIMIT].value.num == MAX_LOCK_MEMORY);
    assert(s.status[LTM_LOCK_MEMORY_CURR].value.num == 0);
}

int main(int argc, const char *argv[]) {

    parse_args(argc, argv);

    int r;

    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, MAX_LOCKS, MAX_LOCK_MEMORY, dbpanic);
    CKERR(r);

    do_ltm_status(ltm);

    toku_ltm_close(ltm);

    return 0;
}

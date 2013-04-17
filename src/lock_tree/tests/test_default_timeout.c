// verify that the lock tree global timeout APIs work

#include "test.h"

int main(int argc, const char *argv[]) {
    int r;

    uint32_t max_locks = 2;
    uint64_t max_lock_memory = 4096;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
        if (strcmp(argv[i], "--max_locks") == 0 && i+1 < argc) {
            max_locks = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--max_lock_memory") == 0 && i+1 < argc) {
            max_lock_memory = atoi(argv[++i]);
            continue;
        }        
        assert(0);
    }

    // setup
    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic, get_compare_fun_from_db);
    assert(r == 0 && ltm);

    uint64_t target_wait_time, the_wait_time;

    toku_ltm_get_lock_wait_time(ltm, &the_wait_time);
    assert(the_wait_time == 0);

    target_wait_time = 1*1000 + 0;
    toku_ltm_set_lock_wait_time(ltm, target_wait_time);
    toku_ltm_get_lock_wait_time(ltm, &the_wait_time);
    assert(the_wait_time == target_wait_time);

    target_wait_time = 2*1000 + 3;
    toku_ltm_set_lock_wait_time(ltm, target_wait_time);
    toku_ltm_get_lock_wait_time(ltm, &the_wait_time);
    assert(the_wait_time == target_wait_time);
    
    target_wait_time = ~0;
    toku_ltm_set_lock_wait_time(ltm, target_wait_time);
    toku_ltm_get_lock_wait_time(ltm, &the_wait_time);
    assert(the_wait_time == target_wait_time);

    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}

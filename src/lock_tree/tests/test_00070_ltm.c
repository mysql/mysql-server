/* Test for a memory leak from just closing the lock tree manager (should close
   all lock trees. */

#include <toku_portability.h>
#include <fcntl.h>
#include "test.h"
#include <unistd.h>

int r;
toku_lock_tree* lt [10] = {0};
toku_ltm*       ltm = NULL;
DB*             db  = (DB*)1;
u_int32_t max_locks = 10;
BOOL duplicates = FALSE;
int  nums[10000];

static void setup_ltm(void) {
    assert(!ltm);
    r = toku_ltm_create(&ltm, max_locks, dbpanic,
                        get_compare_fun_from_db, get_dup_compare_from_db,
                        toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(ltm);
}

static void setup_tree(BOOL dups, size_t index, DICTIONARY_ID dict_id) {
    assert(!lt[index] && ltm);
    r = toku_ltm_get_lt(ltm, &lt[index], dups, dict_id);
    CKERR(r);
    assert(lt[index]);
}


static void close_ltm(void) {
    assert(ltm);
    r = toku_ltm_close(ltm);
    CKERR(r);
    u_int32_t i = 0;
    for (i = 0; i < sizeof(lt)/sizeof(*lt); i++) { lt[i] = NULL; }
    ltm = NULL;
}

static void run_test(BOOL dups) {
    DICTIONARY_ID dict_id1 = {0};
    DICTIONARY_ID dict_id2 = {1};
    DICTIONARY_ID dict_id3 = dict_id1;


    setup_ltm();
    setup_tree(dups, 0, dict_id1);
    setup_tree(dups, 1, dict_id1);
    assert(lt[0] == lt[1]);
    
    setup_tree(dups, 2, dict_id2);
    assert(lt[0] != lt[2]);
    setup_tree(dups, 3, dict_id3);
    assert(lt[0] == lt[3]);
    
    toku_ltm_invalidate_lt(ltm, dict_id1);
    setup_tree(dups, 4, dict_id1);
    assert(lt[0] != lt[4]);
    setup_tree(dups, 5, dict_id1);
    assert(lt[0] != lt[5]);
    assert(lt[4] == lt[5]);
    
    close_ltm();
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    compare_fun = intcmp;
    dup_compare = intcmp;

    system("rm -rf " TESTDIR);
    toku_os_mkdir(TESTDIR, S_IRWXU|S_IRWXG|S_IRWXO);

    run_test(FALSE);
    run_test(TRUE);

    return 0;
}

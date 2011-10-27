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
enum { MAX_LT_LOCKS = 10 };
uint32_t max_locks = MAX_LT_LOCKS;
uint64_t max_lock_memory = MAX_LT_LOCKS*64;
int  nums[10000];

static void setup_ltm(void) {
    assert(!ltm);
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic, get_compare_fun_from_db);
    CKERR(r);
    assert(ltm);
}

static void setup_tree(size_t index, DICTIONARY_ID dict_id) {
    assert(!lt[index] && ltm);
    r = toku_ltm_get_lt(ltm, &lt[index], dict_id, NULL);
    CKERR(r);
    assert(lt[index]);
}


static void close_ltm(void) {
    assert(ltm);
    r = toku_ltm_close(ltm);
    CKERR(r);
    uint32_t i = 0;
    for (i = 0; i < sizeof(lt)/sizeof(*lt); i++) { lt[i] = NULL; }
    ltm = NULL;
}

static void run_test(void) {
    DICTIONARY_ID dict_id1 = {1};
    DICTIONARY_ID dict_id2 = {2};
    DICTIONARY_ID dict_id3 = dict_id1;


    setup_ltm();
    setup_tree(0, dict_id1);
    setup_tree(1, dict_id1);
    assert(lt[0] == lt[1]);
    
    setup_tree(2, dict_id2);
    assert(lt[0] != lt[2]);
    setup_tree(3, dict_id3);
    assert(lt[0] == lt[3]);
    
    toku_ltm_invalidate_lt(ltm, dict_id1);
    setup_tree(4, dict_id1);
    assert(lt[0] != lt[4]);
    setup_tree(5, dict_id1);
    assert(lt[0] != lt[5]);
    assert(lt[4] == lt[5]);
    
    close_ltm();
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    compare_fun = intcmp;

    r = system("rm -rf " TESTDIR);
    CKERR(r);
    toku_os_mkdir(TESTDIR, S_IRWXU|S_IRWXG|S_IRWXO);

    run_test();

    return 0;
}

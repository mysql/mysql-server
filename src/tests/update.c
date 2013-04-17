/* test the update functionality. */

#include "test.h"

DB_ENV *env;

// the commands are: byte 1 is "nop" "add" or "del".  Byte 2 is the amount to add.
enum cmd { CNOP, CADD, CDEL };

static int increment_update (DB *db __attribute__((__unused__)),
                             const DBT *key __attribute__((__unused__)),
                             const DBT *old_val, const DBT *extra,
                             void (*set_val)(const DBT *new_val,
                                             void *set_extra),
                             void *set_extra) {
    assert (extra->size==2);
    assert (old_val->size==4);
    unsigned char *extra_data = extra->data;
    switch ((enum cmd)(extra_data[0])) {
    case CNOP:
        return 0;
    case CADD: {
        unsigned int data = *(unsigned int*)old_val->data;
        data += extra_data[1];
        DBT new_val = {.data=&data, .size=4, .ulen=0, .flags=0};
        set_val(&new_val, set_extra);
        return 0;
    }
    case CDEL:
        set_val(NULL, set_extra);
        return 0;
    }
    assert(0); // enumeration failed.
}

static void setup (void) {
    { int r = system("rm -rf " ENVDIR);                       CKERR(r); }
    { int r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);   CKERR(r); }
    { int r=db_env_create(&env, 0);                           CKERR(r); }
    env->set_errfile(env, stderr);
    env->set_update(env, increment_update);
}

static void cleanup (void) {
    { int r = env->close(env, 0);                             CKERR(r); }
}

int test_main (int argc __attribute__((__unused__)), char *const argv[] __attribute__((__unused__))) {

    setup();
    cleanup();
    return 0;
}

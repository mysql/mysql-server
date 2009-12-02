/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>
#include <stdlib.h>

static int do_recover;
static int do_crash;
static char fileop;
static int choices['H'-'A'+1];
const int num_choices = sizeof(choices)/sizeof(choices[0]);
static DB_TXN *txn;
const char *oldname = "oldfoo";
const char *newname = "newfoo";
DB_ENV *env;
DB     *db;
static int crash_during_checkpoint;
static char *cmd;

#define usage() assert(FALSE)
#if 0
static void
usage(void) {
    fprintf(stderr, "Usage:\n%s [-v|-q]* [-h] (-c|-r) -O fileop -A# -B# -C# -D# -E# [-F# -G#]\n"
                    "  fileop = c/r/d (create/rename/delete)\n"
                    "  Where # is a single digit number > 0.\n"
                    "  A-F are required for fileop=create\n"
                    "  A-H are required for fileop=delete, fileop=rename\n", cmd);
    exit(1);
}
#endif


enum { CLOSE_TXN_COMMIT, CLOSE_TXN_ABORT, CLOSE_TXN_NONE };
enum {CREATE_CREATE, CREATE_CHECKPOINT, CREATE_COMMIT_NEW,
      CREATE_COMMIT_NEW_CHECKPOINT,     CREATE_COMMIT_CHECKPOINT_NEW,
      CREATE_CHECKPOINT_COMMIT_NEW};
static int
get_x_choice(char c, int possibilities) {
    assert(c < 'A' + num_choices);
    assert(c >= 'A');
    int choice = choices[c-'A'];
    if (choice >= possibilities)
        usage();
    return choice;
}

//return 0 or 1
static int
get_bool_choice(char c) {
    return get_x_choice(c, 2);
}

static int
get_choice_first_create_unrelated_txn(void) {
    return get_bool_choice('A');
}

static int
get_choice_do_checkpoint_after_fileop(void) {
    return get_bool_choice('B');
}

static int
get_choice_txn_close_type(void) {
    return get_x_choice('C', 3);
}

static int
get_choice_close_txn_before_checkpoint(void) {
    int choice = get_bool_choice('D');
    //Can't do checkpoint related thing without checkpoint
    if (choice)
        assert(get_choice_do_checkpoint_after_fileop());
    return choice;
}

static int
get_choice_crash_checkpoint_in_callback(void) {
    int choice = get_bool_choice('E');
    //Can't do checkpoint related thing without checkpoint
    if (choice)
        assert(get_choice_do_checkpoint_after_fileop());
    return choice;
}

static int
get_choice_flush_log_before_crash(void) {
    return get_bool_choice('F');
}

static int
get_choice_create_type(void) {
    return get_x_choice('G', 6);
}

static int
get_choice_txn_does_open_close_before_fileop(void) {
    return get_bool_choice('H');
}

static void
do_args(int argc, char *argv[]) {
    cmd = argv[0];
    int i;
    //Clear
    for (i = 0; i < num_choices; i++) {
        choices[i] = -1;
    }

    char c;
    while ((c = getopt(argc, argv, "vqhcrO:A:B:C:D:E:F:G:H:")) != -1) {
	switch(c) {
        case 'v':
	    verbose++;
            break;
        case 'q':
            verbose--;
	    if (verbose<0) verbose=0;
            break;
        case 'h':
        case '?':
            usage();
            break;
        case 'c':
            do_crash = 1;
            break;
        case 'r':
            do_recover = 1;
            break;
        case 'O':
            if (fileop != '\0')
                usage();
            fileop = optarg[0];
            switch (fileop) {
                case 'c':
                case 'r':
                case 'd':
                    break;
                default:
                    usage();
                    break;
            }
            break;
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G':
        case 'H':
            if (fileop == '\0')
                usage();
            int num = atoi(optarg);
            if (num < 0 || num > 9)
                usage();
            choices[c - 'A'] = num;
            break;
	default:
            usage();
            break;
	}
    }
    if (argc!=optind) { usage(); exit(1); }

    for (i = 0; i < num_choices; i++) {
        if (i >= 'G' - 'A' && fileop == 'c')
            break;
        if (choices[i] == -1)
            usage();
    }
    assert(!do_recover || !do_crash);
    assert(do_recover || do_crash);
}

static void UU() crash_it(void) {
    int r;
    if (get_choice_flush_log_before_crash()) {
        r = env->log_flush(env, NULL); //TODO: USe a real DB_LSN* instead of NULL
        CKERR(r);
    }
    fprintf(stderr, "HAPPY CRASH\n");
    fflush(stdout);
    fflush(stderr);
    int zero = 0;
    int divide_by_zero = 1/zero;
    printf("force use of %d\n", divide_by_zero);
    fflush(stdout);
    fflush(stderr);
}

static void checkpoint_callback_maybe_crash(void * UU(extra)) {
    if (crash_during_checkpoint)
        crash_it();
}

static void env_startup(void) {
    int r;
    int recover_flag = do_crash ? 0 : DB_RECOVER;
    if (do_crash) {
        db_env_set_checkpoint_callback(checkpoint_callback_maybe_crash, NULL);
        r = system("rm -rf " ENVDIR);                                                                     CKERR(r);
        r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);                                               CKERR(r);
    }
    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | recover_flag;
    r = db_env_create(&env, 0);
    CKERR(r);
    env->set_errfile(env, stderr);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    //Disable auto-checkpointing.
    r = env->checkpointing_set_period(env, 0);
    CKERR(r);
}

static void
env_shutdown(void) {
    int r;
    r = env->close(env, 0);
    CKERR(r);
}

static void
maybe_make_oldest_living_txn(void) {
    if (get_choice_first_create_unrelated_txn()) {
        // create a txn that never closes, forcing recovery to run from the beginning of the log
        DB_TXN *oldest_living_txn;
        int r;
        r = env->txn_begin(env, NULL, &oldest_living_txn, 0);
        CKERR(r);
    }
}

static void
make_txn(void) {
    int r;
    r = env->txn_begin(env, NULL, &txn, 0);
    CKERR(r);
}

static void
fcreate(void) {
    int r;
    r = db_create(&db, env, 0);                                                             
    CKERR(r);
    r = db->open(db, txn, oldname, NULL, DB_BTREE, DB_CREATE|DB_EXCL, 0666);
    CKERR(r);
    r = db->close(db, 0);
    CKERR(r);
}

static void
fdelete(void) {
    int r;
    r = env->dbremove(env, txn, oldname, NULL, 0);
    CKERR(r);
}

static void
frename(void) {
    int r;
    r = env->dbrename(env, txn, oldname, NULL, newname, 0);
    CKERR(r);
}

static void
close_txn(int type) {
    int r;
    assert(txn);
    if (type==CLOSE_TXN_COMMIT) {
        //commit
        r = txn->commit(txn, 0);
        CKERR(r);
        txn = NULL;
    }
    else if (type == CLOSE_TXN_ABORT) {
        //abort
        r = txn->abort(txn);
        CKERR(r);
        txn = NULL;
    }
    else
        assert(type == CLOSE_TXN_NONE);
}

static void
checkpoint(void) {
    int r;
    r = env->txn_checkpoint(env, 0, 0, 0);
    CKERR(r);
}

static void
create_and_crash(void) {
    //Make txn
    make_txn();
    //fcreate
    fcreate();

    if (get_choice_do_checkpoint_after_fileop()) {
        crash_during_checkpoint = get_choice_crash_checkpoint_in_callback();
        if (get_choice_close_txn_before_checkpoint())
            close_txn(get_choice_txn_close_type());
        checkpoint();
        if (!get_choice_close_txn_before_checkpoint())
            close_txn(get_choice_txn_close_type());
    }
    else {
        crash_during_checkpoint = get_choice_crash_checkpoint_in_callback();
        assert(!crash_during_checkpoint);
        close_txn(get_choice_txn_close_type());
    }
}

static void
create_and_maybe_checkpoint_and_or_close_after_create(void) {
    fcreate();
    switch (get_choice_create_type()) {
        case (CREATE_CREATE): //Just create
            break;
        case (CREATE_CHECKPOINT): //Create then checkpoint
            checkpoint();
            break;
        case (CREATE_COMMIT_NEW): //Create then commit
            close_txn(CLOSE_TXN_COMMIT);
            make_txn();
            break;
        case (CREATE_COMMIT_NEW_CHECKPOINT): //Create then commit then create new txn then checkpoint
            close_txn(CLOSE_TXN_COMMIT);
            make_txn();
            checkpoint();
            break;
        case (CREATE_COMMIT_CHECKPOINT_NEW): //Create then commit then checkpoint then create new txn
            close_txn(CLOSE_TXN_COMMIT);
            checkpoint();
            make_txn();
            break;
        case (CREATE_CHECKPOINT_COMMIT_NEW): //Create then checkpoint then commit then create new txn
            checkpoint();
            close_txn(CLOSE_TXN_COMMIT);
            make_txn();
            break;
        default:
            assert(FALSE);
            break;
    }
}

static void
maybe_open_and_close_file_again_before_fileop(void) {
    if (get_choice_txn_does_open_close_before_fileop()) {
        int r;
        r = db_create(&db, env, 0);                                                             
        CKERR(r);
        r = db->open(db, txn, oldname, NULL, DB_BTREE, 0, 0666);
        CKERR(r);
        r = db->close(db, 0);
        CKERR(r);
    }
}

static void
delete_and_crash(void) {
    //Make txn
    make_txn();
    //fcreate
    create_and_maybe_checkpoint_and_or_close_after_create();

    maybe_open_and_close_file_again_before_fileop();

    fdelete();
    if (get_choice_do_checkpoint_after_fileop()) {
        crash_during_checkpoint = get_choice_crash_checkpoint_in_callback();
        if (get_choice_close_txn_before_checkpoint())
            close_txn(get_choice_txn_close_type());
        checkpoint();
        if (!get_choice_close_txn_before_checkpoint())
            close_txn(get_choice_txn_close_type());
    }
    else {
        crash_during_checkpoint = get_choice_crash_checkpoint_in_callback();
        assert(!crash_during_checkpoint);
        close_txn(get_choice_txn_close_type());
    }
}

static void
rename_and_crash(void) {
    //Make txn
    make_txn();
    //fcreate
    create_and_maybe_checkpoint_and_or_close_after_create();

    maybe_open_and_close_file_again_before_fileop();

    frename();
    if (get_choice_do_checkpoint_after_fileop()) {
        crash_during_checkpoint = get_choice_crash_checkpoint_in_callback();
        if (get_choice_close_txn_before_checkpoint())
            close_txn(get_choice_txn_close_type());
        checkpoint();
        if (!get_choice_close_txn_before_checkpoint())
            close_txn(get_choice_txn_close_type());
    }
    else {
        crash_during_checkpoint = get_choice_crash_checkpoint_in_callback();
        assert(!crash_during_checkpoint);
        close_txn(get_choice_txn_close_type());
    }
}


static void
execute_and_crash(void) {
    maybe_make_oldest_living_txn();
    //split into create/delete/rename
    if (fileop=='c')
        create_and_crash();
    else if (fileop == 'd')
        delete_and_crash();
    else {
       assert(fileop == 'r');
       rename_and_crash();
    }
    crash_it();
}

static int
did_create_commit_early(void) {
    int r;
    switch (get_choice_create_type()) {
        case (CREATE_CREATE): //Just create
        case (CREATE_CHECKPOINT): //Create then checkpoint
            r = 0;
            break;
        case (CREATE_COMMIT_NEW): //Create then commit
        case (CREATE_COMMIT_NEW_CHECKPOINT): //Create then commit then create new txn then checkpoint
        case (CREATE_COMMIT_CHECKPOINT_NEW): //Create then commit then checkpoint then create new txn
        case (CREATE_CHECKPOINT_COMMIT_NEW): //Create then checkpoint then commit then create new txn
            r = 1;
            break;
        default:
            assert(FALSE);
    }
    return r;
}

static void
verify_file_exists(const char *name, int should_exist) {
    int r;
    r = db_create(&db, env, 0);                                                             
    CKERR(r);
    r = db->open(db, NULL, name, NULL, DB_BTREE, 0, 0666);
    if (should_exist)
        CKERR(r);
    else
        CKERR2(r, ENOENT);
    r = db->close(db, 0);
    CKERR(r);
}

static int
fileop_did_commit(void) {
    return get_choice_txn_close_type() == CLOSE_TXN_COMMIT &&
           (!get_choice_do_checkpoint_after_fileop() ||
            !get_choice_crash_checkpoint_in_callback() ||
            get_choice_close_txn_before_checkpoint());
}

static void
recover_and_verify(void) {
    //Recovery was done during env_startup
    int expect_old_name = 0;
    int expect_new_name = 0;
    if (fileop=='c') {
        expect_old_name = fileop_did_commit();
    }
    else if (fileop == 'd') {
        expect_old_name = did_create_commit_early() && !fileop_did_commit();
    }
    else {
        //Wrong? if checkpoint AND crash during checkpoint
        if (fileop_did_commit())
            expect_new_name = 1;
        else if (did_create_commit_early())
            expect_old_name = 1;
    }
    verify_file_exists(oldname, expect_old_name);
    verify_file_exists(newname, expect_new_name);
    env_shutdown();
}

int
test_main(int argc, char *argv[]) {
    crash_during_checkpoint = 0; //Do not crash during checkpoint (possibly during recovery).
    do_args(argc, argv);
    env_startup();
    if (do_crash)
        execute_and_crash();
    else
        recover_and_verify();
    return 0;
}



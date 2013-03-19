/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#ident "$Id$"

/* Purpose of this test is to verify the basic functioning
 * of the engine status functions.
 */


#include "test.h"
#include <db.h>
#include "toku_time.h"

static DB_ENV *env;

#define FLAGS_NOLOG DB_INIT_LOCK|DB_INIT_MPOOL|DB_CREATE|DB_PRIVATE
#define FLAGS_LOG   FLAGS_NOLOG|DB_INIT_TXN|DB_INIT_LOG

static int mode = S_IRWXU+S_IRWXG+S_IRWXO;

static void test_shutdown(void);

static void
test_shutdown(void) {
    int r;
    r=env->close(env, 0); CKERR(r);
    env = NULL;
}

static void
setup (uint32_t flags) {
    int r;
    if (env)
        test_shutdown();
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    r=db_env_create(&env, 0); 
    CKERR(r);
    env->set_errfile(env, stderr);
    r=env->open(env, TOKU_TEST_FILENAME, flags, mode); 
    CKERR(r);
}


static void
print_raw(TOKU_ENGINE_STATUS_ROW row) {
    printf("keyname is %s, type is %d, legend is %s\n",
           row->keyname,
           row->type,
           row->legend);
}    

static void
status_format_time(const time_t *timer, char *buf) {
    ctime_r(timer, buf);
    size_t len = strlen(buf);
    assert(len < 26);
    char end;

    assert(len>=1);
    end = buf[len-1];
    while (end == '\n' || end == '\r') {
        buf[len-1] = '\0';
        len--;
        assert(len>=1);
        end = buf[len-1];
    }
}


int
test_main (int argc, char * const argv[]) {
    uint64_t nrows;
    uint64_t max_rows;
    fs_redzone_state redzone_state;
    uint64_t panic;
    const int panic_string_len = 1024;
    char panic_string[panic_string_len];

    //    char buf[bufsiz] = {'\0'};
    parse_args(argc, argv);
    setup(FLAGS_LOG);
    env->txn_checkpoint(env, 0, 0, 0);

    env->get_engine_status_num_rows(env, &max_rows);
    TOKU_ENGINE_STATUS_ROW_S mystat[max_rows];
    int r = env->get_engine_status (env, mystat, max_rows, &nrows, &redzone_state, &panic, panic_string, panic_string_len, TOKU_ENGINE_STATUS);
    assert(r==0);

    if (verbose) {
        printf("First all the raw fields:\n");
        for (uint64_t i = 0; i < nrows; i++) {
            printf("%s        ", mystat[i].keyname);
            printf("%s       ", mystat[i].legend);
            printf("type=%d  val = ", mystat[i].type);
            switch(mystat[i].type) {
            case FS_STATE:
                printf("fs_state not supported yet, code is %" PRIu64 "\n", mystat[i].value.num);
                break;
            case UINT64:
                printf("%" PRIu64 "\n", mystat[i].value.num);
                break;
            case CHARSTR:
                printf("%s\n", mystat[i].value.str);
                break;
            case UNIXTIME:
                {
                    char tbuf[26];
                    status_format_time((time_t*)&mystat[i].value.num, tbuf);
                    printf("%s\n", tbuf);
                }
                break;
            case TOKUTIME:
                {
                    double t = tokutime_to_seconds(mystat[i].value.num);
                    printf("%.6f\n", t);
                }
                break;
            default:
                printf("UNKNOWN STATUS TYPE:\n");
                print_raw(&mystat[i]);
                break;
            }
        }

        printf("\n\n\n\n\nNow as reported by get_engine_status_text():\n\n");

        int bufsiz = nrows * 128;   // assume 128 characters per row
        char buff[bufsiz];  
        r = env->get_engine_status_text(env, buff, bufsiz);
        printf("%s", buff);

        printf("\n\n\n\n\nFinally, print as reported by test utility print_engine_status()\n");

        print_engine_status(env);

        printf("That's all, folks.\n");
    }
    test_shutdown();
    return 0;
}

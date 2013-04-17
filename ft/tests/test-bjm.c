/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: test-kibbutz2.c 43762 2012-05-22 16:17:53Z yfogel $"
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "background_job_manager.h"
#include "includes.h"
#include "test.h"

BACKGROUND_JOB_MANAGER bjm; 

static void *finish_bjm(void *arg) {
    bjm_wait_for_jobs_to_finish(bjm);
    return arg;
}


static void bjm_test(void) {
    int r = 0;
    bjm = NULL;
    bjm_init(&bjm);
    // test simple add/remove of background job works
    r = bjm_add_background_job(bjm);
    assert_zero(r);
    bjm_remove_background_job(bjm);
    bjm_wait_for_jobs_to_finish(bjm);
    // assert that you cannot add a background job
    // without resetting bjm after waiting 
    // for finish
    r = bjm_add_background_job(bjm);
    assert(r != 0);
    // test that after a reset, we can resume adding background jobs
    bjm_reset(bjm);
    r = bjm_add_background_job(bjm);
    assert_zero(r);
    bjm_remove_background_job(bjm);    
    bjm_wait_for_jobs_to_finish(bjm);

    bjm_reset(bjm);
    r = bjm_add_background_job(bjm);
    assert_zero(r);        
    toku_pthread_t tid;    
    r = toku_pthread_create(&tid, NULL, finish_bjm, NULL); 
    assert_zero(r);
    usleep(2*1024*1024);
    // should return non-zero because tid is waiting 
    // for background jobs to finish
    r = bjm_add_background_job(bjm);
    assert(r != 0);
    bjm_remove_background_job(bjm);
    void *ret;
    r = toku_pthread_join(tid, &ret); 
    assert_zero(r);
    
    bjm_destroy(bjm);
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);
    
    bjm_test();
    if (verbose) printf("test ok\n");
    return 0;
}


